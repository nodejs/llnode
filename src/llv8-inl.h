#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

#include <cinttypes>
#include "llv8.h"

namespace llnode {
namespace v8 {

template <>
inline double LLV8::LoadValue<double>(int64_t addr, Error& err) {
  return LoadDouble(addr, err);
}

template <class T>
inline T LLV8::LoadValue(int64_t addr, Error& err) {
  int64_t ptr;
  ptr = LoadPtr(addr, err);
  if (err.Fail()) return T();

  T res = T(this, ptr);
  if (!res.Check()) {
    // TODO(joyeecheung): use Error::Failure() to report information when
    // there is less noise from here.
    err = Error(true, "Invalid value");
    return T();
  }

  return res;
}


inline bool Smi::Check() const {
  return (raw() & v8()->smi()->kTagMask) == v8()->smi()->kTag;
}


inline int64_t Smi::GetValue() const {
  return raw() >> (v8()->smi()->kShiftSize + v8()->smi()->kTagMask);
}


inline bool HeapObject::Check() const {
  return (raw() & v8()->heap_obj()->kTagMask) == v8()->heap_obj()->kTag;
}

int64_t HeapObject::LeaField(int64_t off) const {
  return raw() - v8()->heap_obj()->kTag + off;
}


inline int64_t HeapObject::LoadField(int64_t off, Error& err) {
  return v8()->LoadPtr(LeaField(off), err);
}


template <>
inline double HeapObject::LoadFieldValue<double>(int64_t off, Error& err) {
  return v8()->LoadValue<double>(LeaField(off), err);
}


template <class T>
inline T HeapObject::LoadFieldValue(int64_t off, Error& err) {
  T res = v8()->LoadValue<T>(LeaField(off), err);
  if (err.Fail()) return T();
  if (!res.Check()) {
    err = Error::Failure("Invalid field value %s at 0x%016" PRIx64,
                         T::ClassName(), off);
    return T();
  }

  return res;
}


inline int64_t HeapObject::GetType(Error& err) {
  HeapObject obj = GetMap(err);
  if (err.Fail()) return -1;

  Map map(obj);
  return map.GetType(err);
}


inline bool HeapObject::ISJSErrorType(Error& err) {
  int64_t type = GetType(err);
  if (type == v8()->types()->kJSErrorType) return true;

  // NOTE (mmarchini): We don't have a JSErroType constant on Node.js v6.x,
  // thus we try to guess if the object is an Error object by checking if its
  // name is Error. Should work most of the time.
  if (!JSObject::IsObjectType(v8(), type)) return false;
  JSObject obj(this);
  if (obj.GetTypeName(err) != "Error" || err.Fail()) return false;

  return true;
}


inline int64_t Map::GetType(Error& err) {
  int64_t type =
      v8()->LoadUnsigned(LeaField(v8()->map()->kInstanceAttrsOffset), 2, err);
  if (err.Fail()) return -1;

  return type & v8()->map()->kMapTypeMask;
}


inline JSFunction JSFrame::GetFunction(Error& err) {
  return v8()->LoadValue<JSFunction>(raw() + v8()->frame()->kFunctionOffset,
                                     err);
}


inline int64_t JSFrame::LeaParamSlot(int slot, int count) const {
  return raw() + v8()->frame()->kArgsOffset +
         (count - slot - 1) * v8()->common()->kPointerSize;
}


inline Value JSFrame::GetReceiver(int count, Error& err) {
  return GetParam(-1, count, err);
}


inline Value JSFrame::GetParam(int slot, int count, Error& err) {
  int64_t addr = LeaParamSlot(slot, count);
  return v8()->LoadValue<Value>(addr, err);
}


inline std::string JSFunction::Name(Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return std::string();

  return info.ProperName(err);
}


inline bool Map::IsDictionary(Error& err) {
  int64_t field = BitField3(err);
  if (err.Fail()) return false;

  return (field & (1 << v8()->map()->kDictionaryMapShift)) != 0;
}


inline int64_t Map::NumberOfOwnDescriptors(Error& err) {
  int64_t field = BitField3(err);
  if (err.Fail()) return false;

  // Skip EnumLength
  field &= v8()->map()->kNumberOfOwnDescriptorsMask;
  field >>= v8()->map()->kNumberOfOwnDescriptorsShift;
  return field;
}


#define ACCESSOR(CLASS, METHOD, OFF, TYPE)       \
  inline TYPE CLASS::METHOD(Error& err) {        \
    return LoadFieldValue<TYPE>(v8()->OFF, err); \
  }


ACCESSOR(HeapObject, GetMap, heap_obj()->kMapOffset, HeapObject)

ACCESSOR(Map, MaybeConstructor, map()->kMaybeConstructorOffset, HeapObject)
ACCESSOR(Map, InstanceDescriptors, map()->kInstanceDescriptorsOffset,
         HeapObject)

inline int64_t Map::BitField3(Error& err) {
  return v8()->LoadUnsigned(LeaField(v8()->map()->kBitField3Offset), 4, err);
}

inline int64_t Map::InstanceType(Error& err) {
  return v8()->LoadUnsigned(LeaField(v8()->map()->kInstanceTypeOffset), 2, err);
}

inline bool Map::IsJSObjectMap(Error& err) {
  return InstanceType(err) >= v8()->types()->kFirstJSObjectType;
}

inline bool Context::IsContext(LLV8* v8, HeapObject heap_object, Error& err) {
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type >= v8->types()->kFirstContextType &&
         type <= v8->types()->kLastContextType;
}

inline int64_t Map::InObjectProperties(Error& err) {
  if (!IsJSObjectMap(err)) {
    err = Error::Failure(
        "Invalid call to Map::InObjectProperties with a non-JsObject type");
    return 0;
  }
  if (v8()->map()->kInObjectPropertiesOffset != -1) {
    return LoadField(v8()->map()->kInObjectPropertiesOffset, err) & 0xff;
  } else {
    // NOTE(mmarchini): V8 6.4 changed semantics for
    // in_objects_properties_offset (see
    // https://chromium-review.googlesource.com/c/v8/v8/+/776720). To keep
    // changes to a minimum on llnode and to comply with V8, we're using the
    // same implementation from
    // https://chromium-review.googlesource.com/c/v8/v8/+/776720/9/src/objects-inl.h#3027.
    int64_t in_object_properties_start_offset =
        LoadField(v8()->map()->kInObjectPropertiesStartOffset, err) & 0xff;
    int64_t instance_size =
        v8()->LoadUnsigned(LeaField(v8()->map()->kInstanceSizeOffset), 1, err);
    return instance_size - in_object_properties_start_offset;
  }
}

inline int64_t Map::ConstructorFunctionIndex(Error& err) {
  if (v8()->map()->kInObjectPropertiesOffset != -1) {
    return LoadField(v8()->map()->kInObjectPropertiesOffset, err) & 0xff;
  } else {
    return LoadField(v8()->map()->kInObjectPropertiesStartOffset, err) & 0xff;
  }
}

inline int64_t Map::InstanceSize(Error& err) {
  return v8()->LoadUnsigned(LeaField(v8()->map()->kInstanceSizeOffset), 1,
                            err) *
         v8()->common()->kPointerSize;
}

ACCESSOR(JSObject, Properties, js_object()->kPropertiesOffset, HeapObject)
ACCESSOR(JSObject, Elements, js_object()->kElementsOffset, HeapObject)

inline bool JSObject::IsObjectType(LLV8* v8, int64_t type) {
  return type == v8->types()->kJSObjectType ||
         type == v8->types()->kJSAPIObjectType ||
         type == v8->types()->kJSErrorType ||
         type == v8->types()->kJSSpecialAPIObjectType;
}

ACCESSOR(HeapNumber, GetValue, heap_number()->kValueOffset, double)

ACCESSOR(JSArray, Length, js_array()->kLengthOffset, Smi)

ACCESSOR(JSRegExp, GetSource, js_regexp()->kSourceOffset, String)

ACCESSOR(JSDate, GetValue, js_date()->kValueOffset, Value)

bool String::IsString(LLV8* v8, HeapObject heap_object, Error& err) {
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type < v8->types()->kFirstNonstringType;
}

inline int64_t String::Representation(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string()->kRepresentationMask;
}


inline int64_t String::Encoding(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string()->kEncodingMask;
}

ACCESSOR(String, Length, string()->kLengthOffset, Smi)

ACCESSOR(Script, Name, script()->kNameOffset, String)
ACCESSOR(Script, LineOffset, script()->kLineOffsetOffset, Smi)
ACCESSOR(Script, Source, script()->kSourceOffset, HeapObject)
ACCESSOR(Script, LineEnds, script()->kLineEndsOffset, HeapObject)

ACCESSOR(SharedFunctionInfo, name, shared_info()->kNameOffset, String)
ACCESSOR(SharedFunctionInfo, InferredName, shared_info()->kInferredNameOffset,
         Value)
ACCESSOR(SharedFunctionInfo, GetScript, shared_info()->kScriptOffset, Script)
ACCESSOR(SharedFunctionInfo, scope_info, shared_info()->kScopeInfoOffset,
         HeapObject)
ACCESSOR(SharedFunctionInfo, name_or_scope_info,
         shared_info()->kNameOrScopeInfoOffset, HeapObject)

HeapObject SharedFunctionInfo::GetScopeInfo(Error& err) {
  if (v8()->shared_info()->kNameOrScopeInfoOffset == -1) return scope_info(err);

  HeapObject maybe_scope_info = name_or_scope_info(err);
  if (!String::IsString(v8(), maybe_scope_info, err)) return maybe_scope_info;

  err = Error::Failure("Couldn't get ScopeInfo");
  return HeapObject();
}

String SharedFunctionInfo::Name(Error& err) {
  if (v8()->shared_info()->kNameOrScopeInfoOffset == -1) return name(err);

  HeapObject maybe_scope_info = name_or_scope_info(err);
  if (err.Fail()) return String();

  if (String::IsString(v8(), maybe_scope_info, err))
    return String(maybe_scope_info);

  if (err.Fail()) return String();

  HeapObject maybe_function_name =
      ScopeInfo(maybe_scope_info).MaybeFunctionName(err);
  if (err.Fail()) return String();

  if (String::IsString(v8(), maybe_function_name, err))
    return maybe_function_name;

  err = Error::Failure("Couldn't get SharedFunctionInfo's name");
  return String();
}

inline int64_t Code::Start() { return LeaField(v8()->code()->kStartOffset); }

inline int64_t Code::Size(Error& err) {
  return LoadField(v8()->code()->kSizeOffset, err) & 0xffffffff;
}

ACCESSOR(Oddball, Kind, oddball()->kKindOffset, Smi)

inline int64_t JSArrayBuffer::BackingStore(Error& err) {
  return LoadField(v8()->js_array_buffer()->kBackingStoreOffset, err);
}

inline int64_t JSArrayBuffer::BitField(Error& err) {
  return LoadField(v8()->js_array_buffer()->kBitFieldOffset, err) & 0xffffffff;
}

ACCESSOR(JSArrayBuffer, ByteLength, js_array_buffer()->kByteLengthOffset, Smi)

ACCESSOR(JSArrayBufferView, Buffer, js_array_buffer_view()->kBufferOffset,
         JSArrayBuffer)
ACCESSOR(JSArrayBufferView, ByteOffset,
         js_array_buffer_view()->kByteOffsetOffset, Smi)
ACCESSOR(JSArrayBufferView, ByteLength,
         js_array_buffer_view()->kByteLengthOffset, Smi)

// TODO(indutny): this field is a Smi on 32bit
inline int64_t SharedFunctionInfo::ParameterCount(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kParameterCountOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  return field;
}

// TODO(indutny): this field is a Smi on 32bit
inline int64_t SharedFunctionInfo::StartPosition(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kStartPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;

  field &= v8()->shared_info()->kStartPositionMask;
  field >>= v8()->shared_info()->kStartPositionShift;
  return field;
}

// TODO (hhellyer): as above, this field is different on 32bit.
inline int64_t SharedFunctionInfo::EndPosition(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kEndPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  field >>= v8()->shared_info()->kEndPositionShift;
  return field;
}

ACCESSOR(JSFunction, Info, js_function()->kSharedInfoOffset,
         SharedFunctionInfo);
ACCESSOR(JSFunction, GetContext, js_function()->kContextOffset, HeapObject);

ACCESSOR(ConsString, First, cons_string()->kFirstOffset, String);
ACCESSOR(ConsString, Second, cons_string()->kSecondOffset, String);

ACCESSOR(SlicedString, Parent, sliced_string()->kParentOffset, String);
ACCESSOR(SlicedString, Offset, sliced_string()->kOffsetOffset, Smi);

ACCESSOR(ThinString, Actual, thin_string()->kActualOffset, String);

ACCESSOR(FixedArrayBase, Length, fixed_array_base()->kLengthOffset, Smi);

inline int64_t FixedTypedArrayBase::GetBase(Error& err) {
  return LoadField(v8()->fixed_typed_array_base()->kBasePointerOffset, err);
}

inline int64_t FixedTypedArrayBase::GetExternal(Error& err) {
  return LoadField(v8()->fixed_typed_array_base()->kExternalPointerOffset, err);
}

inline std::string OneByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->one_byte_string()->kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadString(chars, len.GetValue(), err);
}

inline std::string TwoByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->two_byte_string()->kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadTwoByteString(chars, len.GetValue(), err);
}

inline std::string ConsString::ToString(Error& err) {
  String first = First(err);
  if (err.Fail()) return std::string();

  String second = Second(err);
  if (err.Fail()) return std::string();

  std::string tmp = first.ToString(err);
  if (err.Fail()) return std::string();
  tmp += second.ToString(err);
  if (err.Fail()) return std::string();

  return tmp;
}

inline std::string SlicedString::ToString(Error& err) {
  String parent = Parent(err);
  if (err.Fail()) return std::string();

  // TODO - Remove when we add support for external strings
  // We can't use the offset and length safely if we get "(external)"
  // instead of the original parent string.
  if (parent.Representation(err) == v8()->string()->kExternalStringTag) {
    return parent.ToString(err);
  }

  Smi offset = Offset(err);
  if (err.Fail()) return std::string();

  Smi length = Length(err);
  if (err.Fail()) return std::string();

  std::string tmp = parent.ToString(err);
  if (err.Fail()) return std::string();

  int64_t off = offset.GetValue();
  int64_t len = length.GetValue();
  int64_t tmp_size = tmp.size();
  if (off > tmp_size || len > tmp_size) {
    err = Error::Failure("Failed to display sliced string 0x%016" PRIx64
                         " (offset = 0x%016" PRIx64 ", length = 0x%016" PRIx64
                         ") from parent string 0x%016" PRIx64
                         " (length = 0x%016" PRIx64 ")",
                         raw(), off, len, parent.raw(), tmp_size);
    return std::string(err.GetMessage());
  }
  return tmp.substr(offset.GetValue(), length.GetValue());
}

inline std::string ThinString::ToString(Error& err) {
  String actual = Actual(err);
  if (err.Fail()) return std::string();

  std::string tmp = actual.ToString(err);
  if (err.Fail()) return std::string();

  return tmp;
}

inline int64_t FixedArray::LeaData() const {
  return LeaField(v8()->fixed_array()->kDataOffset);
}

template <class T>
inline T FixedArray::Get(int index, Error& err) {
  int64_t off =
      v8()->fixed_array()->kDataOffset + index * v8()->common()->kPointerSize;
  return LoadFieldValue<T>(off, err);
}

inline Smi DescriptorArray::GetDetails(int index, Error& err) {
  return Get<Smi>(v8()->descriptor_array()->kFirstIndex +
                      index * v8()->descriptor_array()->kSize +
                      v8()->descriptor_array()->kDetailsOffset,
                  err);
}

inline Value DescriptorArray::GetKey(int index, Error& err) {
  return Get<Value>(v8()->descriptor_array()->kFirstIndex +
                        index * v8()->descriptor_array()->kSize +
                        v8()->descriptor_array()->kKeyOffset,
                    err);
}

inline Value DescriptorArray::GetValue(int index, Error& err) {
  return Get<Value>(v8()->descriptor_array()->kFirstIndex +
                        index * v8()->descriptor_array()->kSize +
                        v8()->descriptor_array()->kValueOffset,
                    err);
}

inline bool DescriptorArray::IsDescriptorDetails(Smi details) {
  // node.js <= 7
  if (v8()->descriptor_array()->kPropertyTypeMask != -1) {
    return false;
  }

  // node.js >= 8
  return (details.GetValue() &
          v8()->descriptor_array()->kPropertyLocationMask) ==
         (v8()->descriptor_array()->kPropertyLocationEnum_kDescriptor
          << v8()->descriptor_array()->kPropertyLocationShift);
}
inline bool DescriptorArray::IsFieldDetails(Smi details) {
  // node.js <= 7
  if (v8()->descriptor_array()->kPropertyTypeMask != -1) {
    return (details.GetValue() & v8()->descriptor_array()->kPropertyTypeMask) ==
           v8()->descriptor_array()->kFieldType;
  }

  // node.js >= 8
  return (details.GetValue() &
          v8()->descriptor_array()->kPropertyLocationMask) ==
         (v8()->descriptor_array()->kPropertyLocationEnum_kField
          << v8()->descriptor_array()->kPropertyLocationShift);
}

inline bool DescriptorArray::IsConstFieldDetails(Smi details) {
  // node.js <= 7
  if (v8()->descriptor_array()->kPropertyTypeMask != -1) {
    return (details.GetValue() & v8()->descriptor_array()->kPropertyTypeMask) ==
           v8()->descriptor_array()->kConstFieldType;
  }

  // node.js >= 8
  return false;
}

inline bool DescriptorArray::IsDoubleField(Smi details) {
  int64_t repr = details.GetValue();
  repr &= v8()->descriptor_array()->kRepresentationMask;
  repr >>= v8()->descriptor_array()->kRepresentationShift;

  return repr == v8()->descriptor_array()->kRepresentationDouble;
}

inline int64_t DescriptorArray::FieldIndex(Smi details) {
  return (details.GetValue() & v8()->descriptor_array()->kPropertyIndexMask) >>
         v8()->descriptor_array()->kPropertyIndexShift;
}

inline Value NameDictionary::GetKey(int index, Error& err) {
  int64_t off = v8()->name_dictionary()->kPrefixSize +
                index * v8()->name_dictionary()->kEntrySize +
                v8()->name_dictionary()->kKeyOffset;
  return FixedArray::Get<Value>(off, err);
}

inline Value NameDictionary::GetValue(int index, Error& err) {
  int64_t off = v8()->name_dictionary()->kPrefixSize +
                index * v8()->name_dictionary()->kEntrySize +
                v8()->name_dictionary()->kValueOffset;
  return FixedArray::Get<Value>(off, err);
}

inline int64_t NameDictionary::Length(Error& err) {
  Smi length = FixedArray::Length(err);
  if (err.Fail()) return -1;

  int64_t res = length.GetValue() - v8()->name_dictionary()->kPrefixSize;
  res /= v8()->name_dictionary()->kEntrySize;
  return res;
}

inline JSFunction Context::Closure(Error& err) {
  return FixedArray::Get<JSFunction>(v8()->context()->kClosureIndex, err);
}

inline Value Context::Previous(Error& err) {
  return FixedArray::Get<Value>(v8()->context()->kPreviousIndex, err);
}

inline Value Context::Native(Error& err) {
  return FixedArray::Get<Value>(v8()->context()->kNativeIndex, err);
}

inline bool Context::IsNative(Error& err) {
  Value native = Native(err);
  if (err.Fail()) {
    return false;
  }
  return native.raw() == raw();
}

template <class T>
inline T Context::GetEmbedderData(int64_t index, Error& err) {
  FixedArray embedder_data = FixedArray(*this).Get<FixedArray>(
      v8()->context()->kEmbedderDataIndex, err);
  if (err.Fail()) {
    return T();
  }
  return embedder_data.Get<T>(index, err);
}

HeapObject Context::GetScopeInfo(Error& err) {
  if (v8()->context()->kScopeInfoIndex != -1) {
    return FixedArray::Get<HeapObject>(v8()->context()->kScopeInfoIndex, err);
  }
  JSFunction closure = Closure(err);
  if (err.Fail()) return HeapObject();

  SharedFunctionInfo info = closure.Info(err);
  if (err.Fail()) return HeapObject();

  return info.GetScopeInfo(err);
}

inline Value Context::ContextSlot(int index, Error& err) {
  return FixedArray::Get<Value>(v8()->context()->kMinContextSlots + index, err);
}

inline Smi ScopeInfo::ParameterCount(Error& err) {
  return FixedArray::Get<Smi>(v8()->scope_info()->kParameterCountOffset, err);
}

inline Smi ScopeInfo::StackLocalCount(Error& err) {
  return FixedArray::Get<Smi>(v8()->scope_info()->kStackLocalCountOffset, err);
}

inline Smi ScopeInfo::ContextLocalCount(Error& err) {
  return FixedArray::Get<Smi>(v8()->scope_info()->kContextLocalCountOffset,
                              err);
}

inline String ScopeInfo::ContextLocalName(int index, int param_count,
                                          int stack_count, Error& err) {
  int proper_index = index + stack_count + 1 + param_count;
  proper_index += v8()->scope_info()->kVariablePartIndex;
  return FixedArray::Get<String>(proper_index, err);
}

inline HeapObject ScopeInfo::MaybeFunctionName(Error& err) {
  int proper_index = v8()->scope_info()->kVariablePartIndex +
                     ParameterCount(err).GetValue() + 1 +
                     StackLocalCount(err).GetValue() +
                     (ContextLocalCount(err).GetValue() * 2);
  // NOTE(mmarchini): FunctionName can be stored either in the first, second or
  // third slot after ContextLocalCount. Since there are missing postmortem
  // metadata to determine in which slot its being stored for the present
  // ScopeInfo, we try to find it heuristically.
  int tries = 3;
  while (tries > 0) {
    err = Error();

    HeapObject maybe_function_name =
        FixedArray::Get<HeapObject>(proper_index, err);
    if (err.Success() && String::IsString(v8(), maybe_function_name, err))
      return maybe_function_name;

    tries--;
    proper_index++;
  }

  err = Error::Failure("Couldn't get FunctionName from ScopeInfo");
  return HeapObject();
}

inline bool Oddball::IsHoleOrUndefined(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return false;

  return kind.GetValue() == v8()->oddball()->kTheHole ||
         kind.GetValue() == v8()->oddball()->kUndefined;
}

inline bool Oddball::IsHole(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return false;

  return kind.GetValue() == v8()->oddball()->kTheHole;
}

inline bool JSArrayBuffer::WasNeutered(Error& err) {
  int64_t field = BitField(err);
  if (err.Fail()) return false;

  field &= v8()->js_array_buffer()->kWasNeuteredMask;
  field >>= v8()->js_array_buffer()->kWasNeuteredShift;
  return field != 0;
}

#undef ACCESSOR

}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
