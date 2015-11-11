#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

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
    err = Error::Failure("Invalid value");
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
    err = Error::Failure("Invalid value");
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


inline int64_t Map::GetType(Error& err) {
  int64_t type = LoadField(v8()->map()->kInstanceAttrsOffset, err);
  if (err.Fail()) return -1;
  return type & 0xff;
}


inline JSFunction JSFrame::GetFunction(Error& err) {
  return v8()->LoadValue<JSFunction>(
      raw() + v8()->frame()->kFunctionOffset, err);
}


inline int64_t JSFrame::LeaParamSlot(int slot, int count) const {
  return raw() + v8()->frame()->kArgsOffset +
      (count - slot - 1) * v8()->common()->kPointerSize;
}


inline Value JSFrame::GetReceiver(int count, Error &err) {
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


#define ACCESSOR(CLASS, METHOD, OFF, TYPE)                                    \
    inline TYPE CLASS::METHOD(Error& err) {                                   \
      return LoadFieldValue<TYPE>(v8()->OFF, err);                            \
    }


ACCESSOR(HeapObject, GetMap, heap_obj()->kMapOffset, HeapObject)

ACCESSOR(Map, MaybeConstructor, map()->kMaybeConstructorOffset, HeapObject)
ACCESSOR(Map, InstanceDescriptors, map()->kInstanceDescriptorsOffset, HeapObject)

inline int64_t Map::BitField3(Error& err) {
  return LoadField(v8()->map()->kBitField3Offset, err) & 0xffffffff;
}

inline int64_t Map::InObjectProperties(Error& err) {
  return LoadField(v8()->map()->kInObjectPropertiesOffset, err) & 0xff;
}

inline int64_t Map::InstanceSize(Error& err) {
  return (LoadField(v8()->map()->kInstanceSizeOffset, err) & 0xff) *
      v8()->common()->kPointerSize;
}

ACCESSOR(JSObject, Properties, js_object()->kPropertiesOffset, HeapObject)
ACCESSOR(JSObject, Elements, js_object()->kElementsOffset, HeapObject)

ACCESSOR(HeapNumber, GetValue, heap_number()->kValueOffset, double)

ACCESSOR(JSArray, Length, js_array()->kLengthOffset, Smi)

ACCESSOR(JSDate, GetValue, js_date()->kValueOffset, Value)

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

ACCESSOR(SharedFunctionInfo, Name, shared_info()->kNameOffset, String)
ACCESSOR(SharedFunctionInfo, InferredName, shared_info()->kInferredNameOffset,
         String)
ACCESSOR(SharedFunctionInfo, GetScript, shared_info()->kScriptOffset, Script)
ACCESSOR(SharedFunctionInfo, GetCode, shared_info()->kCodeOffset, Code)
ACCESSOR(SharedFunctionInfo, GetScopeInfo, shared_info()->kScopeInfoOffset,
         HeapObject)

inline int64_t Code::Start() {
  return LeaField(v8()->code()->kStartOffset);
}

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
ACCESSOR(JSArrayBufferView, ByteOffset, js_array_buffer_view()->kByteOffsetOffset,
         Smi)
ACCESSOR(JSArrayBufferView, ByteLength, js_array_buffer_view()->kByteLengthOffset,
         Smi)

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

ACCESSOR(JSFunction, Info, js_function()->kSharedInfoOffset,
         SharedFunctionInfo);
ACCESSOR(JSFunction, GetContext, js_function()->kContextOffset, HeapObject);

ACCESSOR(ConsString, First, cons_string()->kFirstOffset, String);
ACCESSOR(ConsString, Second, cons_string()->kSecondOffset, String);

ACCESSOR(SlicedString, Parent, sliced_string()->kParentOffset, String);
ACCESSOR(SlicedString, Offset, sliced_string()->kOffsetOffset, Smi);

ACCESSOR(FixedArrayBase, Length, fixed_array_base()->kLengthOffset, Smi);

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

  Smi offset = Offset(err);
  if (err.Fail()) return std::string();

  Smi length = Length(err);
  if (err.Fail()) return std::string();

  std::string tmp = parent.ToString(err);
  if (err.Fail()) return std::string();

  return tmp.substr(offset.GetValue(), length.GetValue());
}

inline int64_t FixedArray::LeaData() const {
  return LeaField(v8()->fixed_array()->kDataOffset);
}

template <class T>
inline T FixedArray::Get(int index, Error& err) {
  int64_t off = v8()->fixed_array()->kDataOffset +
      index * v8()->common()->kPointerSize;
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

inline bool DescriptorArray::IsFieldDetails(Smi details) {
  return (details.GetValue() & v8()->descriptor_array()->kPropertyTypeMask) ==
      v8()->descriptor_array()->kFieldType;
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

inline JSObject Context::Global(Error& err) {
  return FixedArray::Get<JSObject>(v8()->context()->kGlobalObjectIndex, err);
}

inline Context Context::Previous(Error& err) {
  return FixedArray::Get<Context>(v8()->context()->kPreviousIndex, err);
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

inline Smi ScopeInfo::ContextGlobalCount(Error& err) {
  return FixedArray::Get<Smi>(v8()->scope_info()->kContextGlobalCountOffset,
      err);
}

inline String ScopeInfo::ContextLocalName(int index, int param_count,
    int stack_count, Error& err) {
  int proper_index = index + stack_count + 1 + param_count;
  proper_index += v8()->scope_info()->kVariablePartIndex;
  return FixedArray::Get<String>(proper_index, err);
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
