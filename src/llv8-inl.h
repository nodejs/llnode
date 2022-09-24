#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

#include <cinttypes>
#include "llv8.h"

namespace llnode {
namespace v8 {

using lldb::addr_t;
using lldb::SBError;

template <typename T>
inline std::string CheckedType<T>::ToString(const char* fmt) {
  if (!Check()) return "???";

  char buf[20];
  snprintf(buf, sizeof(buf), fmt, val_);
  return std::string(buf);
}

template <class T>
inline CheckedType<T> LLV8::LoadUnsigned(int64_t addr, uint32_t byte_size) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
                                                  byte_size, sberr);

  if (sberr.Fail()) {
    PRINT_DEBUG("Failed to load unsigned from v8 memory. Reason: %s",
                sberr.GetCString());
    return CheckedType<T>();
  }

  return CheckedType<T>(value);
}

template <>
inline double LLV8::LoadValue<double>(int64_t addr, Error& err) {
  return LoadDouble(addr, err);
}

template <>
inline int32_t LLV8::LoadValue<int32_t>(int64_t addr, Error& err) {
  return LoadUnsigned(addr, 4, err);
}

template <>
inline CheckedType<int32_t> LLV8::LoadValue<CheckedType<int32_t>>(
    int64_t addr) {
  return LoadUnsigned<int32_t>(addr, 4);
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
#if DEBUG
#define _s typeid(T).name()
#else
#define _s "value"
#endif
    err = Error(true, "The value %lx is not a valid %s", addr, _s);
#undef _s
    return T();
  }

  return res;
}


inline bool Smi::Check() const {
  return valid_ && (raw() & v8()->smi()->kTagMask) == v8()->smi()->kTag;
}


inline int64_t Smi::GetValue() const {
  return raw() >> (v8()->smi()->kShiftSize + v8()->smi()->kTagMask);
}


inline bool HeapObject::Check() const {
  return valid_ &&
         (raw() & v8()->heap_obj()->kTagMask) == v8()->heap_obj()->kTag;
}

inline bool HeapNumber::Check() const {
  if (unboxed_double_) return Value::Check();
  return HeapObject::Check();
}

int64_t HeapObject::LeaField(int64_t off) const {
  return raw() - v8()->heap_obj()->kTag + off;
}


inline int64_t HeapObject::LoadField(int64_t off, Error& err) {
  return v8()->LoadPtr(LeaField(off), err);
}


template <typename T>
inline CheckedType<T> HeapObject::LoadCheckedField(Constant<int64_t> off) {
  RETURN_IF_THIS_INVALID(CheckedType<T>());
  RETURN_IF_INVALID(off, CheckedType<T>());
  return v8()->LoadUnsigned<T>(LeaField(*off), 8);
}


template <>
inline int32_t HeapObject::LoadFieldValue<int32_t>(int64_t off, Error& err) {
  return v8()->LoadValue<int32_t>(LeaField(off), err);
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

inline bool Value::IsSmi(Error& err) {
  Smi smi(*this);
  return smi.Check();
}


inline bool Value::IsScript(Error& err) {
  Smi smi(*this);
  if (smi.Check()) return false;

  HeapObject heap_object(*this);
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type == v8()->types()->kScriptType;
}


inline bool Value::IsScopeInfo(Error& err) {
  Smi smi(*this);
  if (smi.Check()) return false;

  HeapObject heap_object(*this);
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type == v8()->types()->kScopeInfoType;
}


inline bool Value::IsUncompiledData(Error& err) {
  Smi smi(*this);
  if (smi.Check()) return false;

  HeapObject heap_object(*this);
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type == *v8()->types()->kUncompiledDataWithoutPreParsedScopeType ||
         type == *v8()->types()->kUncompiledDataWithPreParsedScopeType;
}


inline bool HeapObject::IsJSErrorType(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return false;
  if (type == v8()->types()->kJSErrorType) return true;

  // NOTE (mmarchini): We don't have a JSErrorType constant on Node.js v6.x,
  // thus we try to guess if the object is an Error object by checking if its
  // name is Error. Should work most of the time.
  if (!JSObject::IsObjectType(v8(), type)) return false;

  JSObject obj(this);
  std::string type_name = obj.GetTypeName(err);
  return err.Success() && type_name == "Error";
}


// TODO(mmarchini): return CheckedType
inline int64_t Map::GetType(Error& err) {
  RETURN_IF_INVALID(v8()->map()->kInstanceAttrsOffset, -1);
  int64_t type = v8()->LoadUnsigned(
      LeaField(*(v8()->map()->kInstanceAttrsOffset)), 2, err);
  if (err.Fail()) return -1;

  return type & v8()->map()->kMapTypeMask;
}


inline JSFunction JSFrame::GetFunction(Error& err) {
  return v8()->LoadValue<JSFunction>(raw() + v8()->frame()->kFunctionOffset,
                                     err);
}


inline int64_t JSFrame::LeaParamSlot(int slot, int count) const {
  // On older versions of V8 with argument adaptor frames (particularly for
  // Node.js v14), parameters are pushed onto the stack in the "reverse" order.
  int64_t offset =
      v8()->frame()->kAdaptorFrame == -1 ? slot + 1 : count - slot - 1;
  return raw() + v8()->frame()->kArgsOffset +
         offset * v8()->common()->kPointerSize;
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
    if (!Check()) return TYPE();                 \
    return LoadFieldValue<TYPE>(v8()->OFF, err); \
  }

#define SAFE_ACCESSOR(CLASS, METHOD, OFF, TYPE)     \
  inline TYPE CLASS::METHOD(Error& err) {           \
    if (!Check()) return TYPE();                    \
    if (!v8()->OFF.Check()) return TYPE();          \
    return LoadFieldValue<TYPE>(*(v8()->OFF), err); \
  }


ACCESSOR(HeapObject, GetMap, heap_obj()->kMapOffset, HeapObject)

ACCESSOR(Map, MaybeConstructor, map()->kMaybeConstructorOffset, HeapObject)
SAFE_ACCESSOR(Map, InstanceDescriptors, map()->kInstanceDescriptorsOffset,
              HeapObject)

SAFE_ACCESSOR(Symbol, Name, symbol()->kNameOffset, HeapObject)

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
  RETURN_IF_THIS_INVALID(-1);
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
         type == v8->types()->kJSPromiseType ||
         type == v8->types()->kJSSpecialAPIObjectType;
}

inline std::string JSObject::GetName(Error& err) {
  v8::HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  v8::Map map(map_obj);
  v8::HeapObject constructor_obj = map.Constructor(err);
  if (err.Fail()) return std::string();

  int64_t constructor_type = constructor_obj.GetType(err);
  if (err.Fail()) return std::string();

  if (constructor_type != v8()->types()->kJSFunctionType) {
    return "no constructor";
  }

  v8::JSFunction constructor(constructor_obj);

  std::string name = constructor.Name(err);
  if (err.Fail()) return std::string();

  return name;
}

template <class T>
inline T JSObject::GetInObjectValue(int64_t size, int index, Error& err) {
  return LoadFieldValue<T>(size + index * v8()->common()->kPointerSize, err);
}

inline HeapNumber JSObject::GetDoubleField(int64_t index, Error err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) HeapNumber();

  Map map(map_obj);
  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return HeapNumber();

  // TODO(mmarchini): Explain why index might be lower than zero.
  if (index < 0) {
    return GetInObjectValue<HeapNumber>(instance_size, index, err);
  }
  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return HeapNumber();

  FixedArray extra_properties(extra_properties_obj);

  return HeapNumber(v8(), extra_properties.Get<double>(index, err));
}

inline const CheckedType<double> HeapNumber::GetValue(Error& err) {
  if (unboxed_double_) return unboxed_value_;
  return GetHeapNumberValue(err);
};

ACCESSOR(HeapNumber, GetHeapNumberValue, heap_number()->kValueOffset, double)

ACCESSOR(JSArray, Length, js_array()->kLengthOffset, Smi)

ACCESSOR(JSRegExp, GetSource, js_regexp()->kSourceOffset, String)

ACCESSOR(JSDate, GetValue, js_date()->kValueOffset, Value)

bool String::IsString(LLV8* v8, HeapObject heap_object, Error& err) {
  if (!heap_object.Check()) return false;

  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return false;

  return type < v8->types()->kFirstNonstringType;
}

inline CheckedType<int64_t> String::Representation(Error& err) {
  RETURN_IF_INVALID((*this), CheckedType<int64_t>());

  int64_t type = GetType(err);
  if (err.Fail()) return CheckedType<int64_t>();
  return type & v8()->string()->kRepresentationMask;
}


inline int64_t String::Encoding(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string()->kEncodingMask;
}

inline CheckedType<int32_t> String::Length(Error& err) {
  RETURN_IF_INVALID((*this), CheckedType<int32_t>());

  CheckedType<int32_t> len = v8()->LoadValue<CheckedType<int32_t>>(
      LeaField(v8()->string()->kLengthOffset));
  RETURN_IF_INVALID(len, CheckedType<int32_t>());

  return len;
}


ACCESSOR(Script, Name, script()->kNameOffset, String)
ACCESSOR(Script, LineOffset, script()->kLineOffsetOffset, Smi)
ACCESSOR(Script, Source, script()->kSourceOffset, String)
ACCESSOR(Script, LineEnds, script()->kLineEndsOffset, HeapObject)

ACCESSOR(SharedFunctionInfo, function_data, shared_info()->kFunctionDataOffset,
         Value)
ACCESSOR(SharedFunctionInfo, name, shared_info()->kNameOffset, String)
ACCESSOR(SharedFunctionInfo, inferred_name, shared_info()->kInferredNameOffset,
         Value)
SAFE_ACCESSOR(SharedFunctionInfo, script_or_debug_info,
              shared_info()->kScriptOrDebugInfoOffset, HeapObject)
ACCESSOR(SharedFunctionInfo, name_or_scope_info,
         shared_info()->kNameOrScopeInfoOffset, HeapObject)

ACCESSOR(UncompiledData, inferred_name, uncompiled_data()->kInferredNameOffset,
         Value)
ACCESSOR(UncompiledData, start_position,
         uncompiled_data()->kStartPositionOffset, int32_t)
ACCESSOR(UncompiledData, end_position, uncompiled_data()->kEndPositionOffset,
         int32_t)

Value SharedFunctionInfo::GetInferredName(Error& err) {
  if (v8()->uncompiled_data()->kInferredNameOffset == -1)
    return inferred_name(err);

  HeapObject maybe_scope_info = GetScopeInfo(err);
  if (!err.Fail()) {
    ScopeInfo scope_info(maybe_scope_info);

    Value maybe_inferred_name = scope_info.MaybeFunctionName(err);
    if (!(err.Fail() && String::IsString(v8(), maybe_inferred_name, err)))
      return maybe_inferred_name;
  }

  err = Error::Ok();
  Value maybe_uncompiled_data = function_data(err);
  if (!maybe_uncompiled_data.IsUncompiledData(err)) {
    PRINT_DEBUG("Couldn't get UncompiledData");
    return Value();
  }

  UncompiledData uncompiled_data(maybe_uncompiled_data);
  return uncompiled_data.inferred_name(err);
}

HeapObject SharedFunctionInfo::GetScopeInfo(Error& err) {
  HeapObject maybe_scope_info = name_or_scope_info(err);
  if (!String::IsString(v8(), maybe_scope_info, err)) return maybe_scope_info;

  err = Error::Failure("Couldn't get ScopeInfo");
  return HeapObject();
}

Script SharedFunctionInfo::GetScript(Error& err) {
  HeapObject maybe_script = script_or_debug_info(err);
  if (maybe_script.IsScript(err)) return maybe_script;

  PRINT_DEBUG("Couldn't get Script in SharedFunctionInfo");
  return Script();
}

String SharedFunctionInfo::Name(Error& err) {
  if (v8()->shared_info()->kNameOrScopeInfoOffset == -1) return name(err);

  HeapObject maybe_scope_info = name_or_scope_info(err);
  if (err.Fail()) return String();

  if (String::IsString(v8(), maybe_scope_info, err))
    return String(maybe_scope_info);

  if (err.Fail()) return String();

  Value maybe_function_name =
      ScopeInfo(maybe_scope_info).MaybeFunctionName(err);
  if (err.Fail()) {
    err = Error::Ok();
    return String();
  }

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

inline CheckedType<uintptr_t> JSArrayBuffer::BackingStore() {
  RETURN_IF_THIS_INVALID(CheckedType<size_t>());

  return LoadCheckedField<uintptr_t>(
      v8()->js_array_buffer()->kBackingStoreOffset);
}

inline CheckedType<size_t> JSArrayBuffer::ByteLength() {
  RETURN_IF_THIS_INVALID(CheckedType<size_t>());

  if (!v8()->js_array_buffer()->IsByteLengthScalar()) {
    Error err;
    Smi len = byte_length(err);
    RETURN_IF_INVALID(len, CheckedType<size_t>());

    return CheckedType<size_t>(len.GetValue());
  }

  return LoadCheckedField<size_t>(v8()->js_array_buffer()->kByteLengthOffset);
}

inline CheckedType<int64_t> JSArrayBuffer::BitField() {
  RETURN_IF_THIS_INVALID(CheckedType<int64_t>());
  CheckedType<int64_t> bit_fields =
      LoadCheckedField<int64_t>(v8()->js_array_buffer()->BitFieldOffset());
  RETURN_IF_INVALID(bit_fields, CheckedType<int64_t>());
  return CheckedType<int64_t>(*bit_fields & 0xffffffff);
}

SAFE_ACCESSOR(JSArrayBuffer, byte_length, js_array_buffer()->kByteLengthOffset,
              Smi)

ACCESSOR(JSArrayBufferView, Buffer, js_array_buffer_view()->kBufferOffset,
         JSArrayBuffer)

inline CheckedType<size_t> JSArrayBufferView::ByteLength() {
  RETURN_IF_THIS_INVALID(CheckedType<size_t>());

  if (!v8()->js_array_buffer_view()->IsByteLengthScalar()) {
    Error err;
    Smi len = byte_length(err);
    RETURN_IF_INVALID(len, CheckedType<size_t>());

    return CheckedType<size_t>(len.GetValue());
  }

  return LoadCheckedField<size_t>(
      v8()->js_array_buffer_view()->kByteLengthOffset);
}

inline CheckedType<size_t> JSArrayBufferView::ByteOffset() {
  RETURN_IF_THIS_INVALID(CheckedType<size_t>());

  if (!v8()->js_array_buffer_view()->IsByteOffsetScalar()) {
    Error err;
    Smi len = byte_offset(err);
    RETURN_IF_INVALID(len, CheckedType<size_t>());

    return CheckedType<size_t>(len.GetValue());
  }

  return LoadCheckedField<size_t>(
      v8()->js_array_buffer_view()->kByteOffsetOffset);
}

SAFE_ACCESSOR(JSArrayBufferView, byte_offset,
              js_array_buffer_view()->kByteOffsetOffset, Smi)
SAFE_ACCESSOR(JSArrayBufferView, byte_length,
              js_array_buffer_view()->kByteLengthOffset, Smi)

inline CheckedType<int64_t> JSTypedArray::base() {
  return LoadCheckedField<int64_t>(v8()->js_typed_array()->kBasePointerOffset);
}

inline CheckedType<int64_t> JSTypedArray::external() {
  return LoadCheckedField<int64_t>(
      v8()->js_typed_array()->kExternalPointerOffset);
}

inline CheckedType<int64_t> JSTypedArray::GetExternal() {
  if (v8()->js_typed_array()->IsDataPointerInJSTypedArray()) {
    PRINT_DEBUG("OHALO");
    return external();
  } else {
    PRINT_DEBUG("NAY");
    // TODO(mmarchini): don't rely on Error
    Error err;
    v8::HeapObject elements_obj = Elements(err);
    RETURN_IF_INVALID(elements_obj, CheckedType<int64_t>());
    v8::FixedTypedArrayBase elements(elements_obj);
    return elements.GetExternal();
  }
}

inline CheckedType<int64_t> JSTypedArray::GetBase() {
  if (v8()->js_typed_array()->IsDataPointerInJSTypedArray()) {
    PRINT_DEBUG("ALOHA");
    return base();
  } else {
    PRINT_DEBUG("NEY");
    // TODO(mmarchini): don't rely on Error
    Error err;
    v8::HeapObject elements_obj = Elements(err);
    RETURN_IF_INVALID(elements_obj, CheckedType<int64_t>());
    v8::FixedTypedArrayBase elements(elements_obj);
    return elements.GetBase();
  }
}

inline CheckedType<uintptr_t> JSTypedArray::GetData() {
  // TODO(mmarchini): don't rely on Error
  Error err;
  v8::JSArrayBuffer buf = Buffer(err);
  if (err.Fail()) return CheckedType<uintptr_t>();

  v8::CheckedType<uintptr_t> data = buf.BackingStore();
  // TODO(mmarchini): be more lenient to failed load
  RETURN_IF_INVALID(data, CheckedType<uintptr_t>());

  if (*data == 0) {
    // The backing store has not been materialized yet.

    CheckedType<int64_t> base = GetBase();
    RETURN_IF_INVALID(base, v8::CheckedType<uintptr_t>());

    CheckedType<int64_t> external = GetExternal();
    RETURN_IF_INVALID(external, v8::CheckedType<uintptr_t>());

    data = v8::CheckedType<uintptr_t>(*base + *external);
  }
  return data;
}


inline ScopeInfo::PositionInfo ScopeInfo::MaybePositionInfo(Error& err) {
  ScopeInfo::PositionInfo position_info = {
      .start_position = 0, .end_position = 0, .is_valid = false};
  auto kPointerSize = v8()->common()->kPointerSize;
  int bytes_offset = kPointerSize * ContextLocalIndex(err);
  if (err.Fail()) return position_info;

  Smi context_local_count = ContextLocalCount(err);
  if (err.Fail()) return position_info;
  bytes_offset += 2 * kPointerSize * context_local_count.GetValue();

  int64_t data_offset =
      v8()->scope_info()->kIsFixedArray ? v8()->fixed_array()->kDataOffset : 0;
  bytes_offset += data_offset;

  int tries = 5;
  while (tries > 0) {
    err = Error();

    Smi maybe_start_position =
        HeapObject::LoadFieldValue<Smi>(bytes_offset, err);
    if (err.Success() && maybe_start_position.IsSmi(err)) {
      bytes_offset += kPointerSize;
      Smi maybe_end_position =
          HeapObject::LoadFieldValue<Smi>(bytes_offset, err);
      if (err.Success() && maybe_end_position.IsSmi(err)) {
        position_info.start_position = maybe_start_position.GetValue();
        position_info.end_position = maybe_end_position.GetValue();
        position_info.is_valid = true;
        return position_info;
      }
    }

    tries--;
    bytes_offset += kPointerSize;
  }
  return position_info;
}

// TODO(indutny): this field is a Smi on 32bit
inline int64_t SharedFunctionInfo::ParameterCount(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kParameterCountOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffff;
  return field;
}

// TODO(indutny): this field is a Smi on 32bit
inline int64_t SharedFunctionInfo::StartPosition(Error& err) {
  if (v8()->uncompiled_data()->kStartPositionOffset != -1) {
    Value maybe_scope_info = name_or_scope_info(err);
    if (err.Fail()) return -1;
    if (maybe_scope_info.IsScopeInfo(err)) {
      ScopeInfo scope_info(maybe_scope_info);
      auto position_info = scope_info.MaybePositionInfo(err);
      if (err.Fail()) return -1;
      if (position_info.is_valid) return position_info.start_position;
    }

    Value maybe_uncompiled_data = function_data(err);
    if (!maybe_uncompiled_data.IsUncompiledData(err)) {
      return 0;
    }

    UncompiledData uncompiled_data(maybe_uncompiled_data);
    return uncompiled_data.start_position(err);
  }

  int64_t field = LoadField(v8()->shared_info()->kStartPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;

  field &= v8()->shared_info()->kStartPositionMask;
  field >>= v8()->shared_info()->kStartPositionShift;
  return field;
}

// TODO (hhellyer): as above, this field is different on 32bit.
inline int64_t SharedFunctionInfo::EndPosition(Error& err) {
  if (v8()->uncompiled_data()->kEndPositionOffset != -1) {
    Value maybe_scope_info = name_or_scope_info(err);
    if (err.Fail()) return -1;
    if (maybe_scope_info.IsScopeInfo(err)) {
      ScopeInfo scope_info(maybe_scope_info);
      auto position_info = scope_info.MaybePositionInfo(err);
      if (err.Fail()) return -1;
      if (position_info.is_valid) return position_info.end_position;
    }

    Value maybe_uncompiled_data = function_data(err);
    if (!maybe_uncompiled_data.IsUncompiledData(err)) {
      err = Error::Failure("Couldn't get ScopeInfo");
      return -1;
    }

    UncompiledData uncompiled_data(maybe_uncompiled_data);
    return uncompiled_data.end_position(err);
  }

  int64_t field = LoadField(v8()->shared_info()->kEndPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  field >>= v8()->shared_info()->kEndPositionShift;
  return field;
}

ACCESSOR(JSFunction, Info, js_function()->kSharedInfoOffset,
         SharedFunctionInfo);
ACCESSOR(JSFunction, GetContext, js_function()->kContextOffset, HeapObject);

SAFE_ACCESSOR(ConsString, First, cons_string()->kFirstOffset, String);
SAFE_ACCESSOR(ConsString, Second, cons_string()->kSecondOffset, String);

ACCESSOR(SlicedString, Parent, sliced_string()->kParentOffset, String);
SAFE_ACCESSOR(SlicedString, Offset, sliced_string()->kOffsetOffset, Smi);

SAFE_ACCESSOR(ThinString, Actual, thin_string()->kActualOffset, String);

ACCESSOR(FixedArrayBase, Length, fixed_array_base()->kLengthOffset, Smi);

inline CheckedType<int64_t> FixedTypedArrayBase::GetBase() {
  return LoadCheckedField<int64_t>(
      v8()->fixed_typed_array_base()->kBasePointerOffset);
}

inline CheckedType<int64_t> FixedTypedArrayBase::GetExternal() {
  return LoadCheckedField<int64_t>(
      v8()->fixed_typed_array_base()->kExternalPointerOffset);
}

inline std::string OneByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->one_byte_string()->kCharsOffset);
  CheckedType<int32_t> len = Length(err);
  RETURN_IF_INVALID(len, std::string());
  return v8()->LoadString(chars, *len, err);
}

inline std::string TwoByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->two_byte_string()->kCharsOffset);
  CheckedType<int32_t> len = Length(err);
  RETURN_IF_INVALID(len, std::string());
  return v8()->LoadTwoByteString(chars, *len, err);
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
  RETURN_IF_INVALID(parent, std::string());

  // TODO - Remove when we add support for external strings
  // We can't use the offset and length safely if we get "(external)"
  // instead of the original parent string.
  CheckedType<int64_t> repr = parent.Representation(err);
  RETURN_IF_INVALID(repr, std::string());
  if (*repr == v8()->string()->kExternalStringTag) {
    return parent.ToString(err);
  }

  Smi offset = Offset(err);
  if (err.Fail()) return std::string();
  RETURN_IF_INVALID(offset, std::string());

  CheckedType<int32_t> length = Length(err);
  RETURN_IF_INVALID(length, std::string());

  std::string tmp = parent.ToString(err);
  if (err.Fail()) return std::string();

  int64_t off = offset.GetValue();
  int64_t tmp_size = tmp.size();
  if (off > tmp_size || *length > tmp_size || *length < 0 || off < 0) {
    err = Error::Failure("Failed to display sliced string 0x%016" PRIx64
                         " (offset = 0x%016" PRIx64
                         ", length = %d) from parent string 0x%016" PRIx64
                         " (length = 0x%016" PRIx64 ")",
                         raw(), off, *length, parent.raw(), tmp_size);
    return std::string(err.GetMessage());
  }
  return tmp.substr(offset.GetValue(), *length);
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

template <class T>
inline T DescriptorArray::Get(int index, int64_t offset) {
  // TODO(mmarchini): shouldn't need Error here.
  Error err;
  RETURN_IF_INVALID(v8()->descriptor_array()->kSize, T());

  index = index * *(v8()->descriptor_array()->kSize);
  if (v8()->descriptor_array()->kFirstIndex.Loaded()) {
    return FixedArray::Get<T>(
        *(v8()->descriptor_array()->kFirstIndex) + index + offset, err);
  } else if (v8()->descriptor_array()->kHeaderSize.Check()) {
    index *= v8()->common()->kPointerSize;
    index += *(v8()->descriptor_array()->kHeaderSize);
    index += (v8()->common()->kPointerSize * offset);
    return LoadFieldValue<T>(index, err);
  } else {
    PRINT_DEBUG(
        "Missing FirstIndex and HeaderSize constants, can't get key from "
        "DescriptorArray");
    return T();
  }
}

inline Smi DescriptorArray::GetDetails(int index) {
  RETURN_IF_INVALID(v8()->descriptor_array()->kDetailsOffset, Smi());
  return Get<Smi>(index, *v8()->descriptor_array()->kDetailsOffset);
}

inline Value DescriptorArray::GetKey(int index) {
  RETURN_IF_INVALID(v8()->descriptor_array()->kKeyOffset, Value());
  return Get<Value>(index, *v8()->descriptor_array()->kKeyOffset);
}

inline Value DescriptorArray::GetValue(int index) {
  RETURN_IF_INVALID(v8()->descriptor_array()->kValueOffset, Value());
  return Get<Value>(index, *v8()->descriptor_array()->kValueOffset);
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
  int64_t data_offset =
      v8()->scope_info()->kIsFixedArray ? v8()->fixed_array()->kDataOffset : 0;
  return HeapObject::LoadFieldValue<Smi>(
      data_offset + v8()->scope_info()->kParameterCountOffset *
                        v8()->common()->kPointerSize,
      err);
}

inline Smi ScopeInfo::ContextLocalCount(Error& err) {
  int64_t data_offset = v8()->scope_info()->kIsFixedArray
                            ? v8()->fixed_array()->kDataOffset
                            : v8()->common()->kPointerSize;
  return HeapObject::LoadFieldValue<Smi>(
      data_offset + v8()->scope_info()->kContextLocalCountOffset *
                        v8()->common()->kPointerSize,
      err);
}

inline int ScopeInfo::ContextLocalIndex(Error& err) {
  int context_local_index = v8()->scope_info()->kVariablePartIndex;
  return context_local_index;
}

inline String ScopeInfo::ContextLocalName(int index, Error& err) {
  int64_t data_offset = v8()->scope_info()->kIsFixedArray
                            ? v8()->fixed_array()->kDataOffset
                            : v8()->common()->kPointerSize;
  int proper_index = data_offset + (ContextLocalIndex(err) + index) *
                                       v8()->common()->kPointerSize;
  if (err.Fail()) return String();
  return HeapObject::LoadFieldValue<String>(proper_index, err);
}

inline HeapObject ScopeInfo::MaybeFunctionName(Error& err) {
  // NOTE(mmarchini): FunctionName can be stored either in the first, second or
  // third slot after ContextLocalCount. Since there are missing postmortem
  // metadata to determine in which slot its being stored for the present
  // ScopeInfo, we try to find it heuristically.
  auto kPointerSize = v8()->common()->kPointerSize;
  HeapObject likely_function_name;
  int bytes_offset = kPointerSize * ContextLocalIndex(err);
  if (err.Fail()) return likely_function_name;

  Smi context_local_count = ContextLocalCount(err);
  if (err.Fail()) return likely_function_name;
  bytes_offset += 2 * kPointerSize * context_local_count.GetValue();

  int64_t data_offset =
      v8()->scope_info()->kIsFixedArray ? v8()->fixed_array()->kDataOffset : 0;
  bytes_offset += data_offset;

  int tries = 5;
  while (tries > 0) {
    err = Error();

    HeapObject maybe_function_name =
        HeapObject::LoadFieldValue<HeapObject>(bytes_offset, err);
    if (err.Success() && String::IsString(v8(), maybe_function_name, err)) {
      likely_function_name = maybe_function_name;
      if (*String(likely_function_name).Length(err) > 0) {
        return likely_function_name;
      }
    }

    tries--;
    bytes_offset += kPointerSize;
  }

  if (likely_function_name.Check()) {
    return likely_function_name;
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

// TODO(mmarchini): return CheckedType
inline bool JSArrayBuffer::WasNeutered(Error& err) {
  CheckedType<int64_t> bit_field = BitField();
  RETURN_IF_INVALID(bit_field, false);

  int64_t field = *bit_field;
  field &= v8()->js_array_buffer()->kWasNeuteredMask;
  field >>= v8()->js_array_buffer()->kWasNeuteredShift;
  return field != 0;
}

inline std::string JSError::stack_trace_property() {
  // TODO (mmarchini): once we have Symbol support we'll need to search for
  // <unnamed symbol>, since the stack symbol doesn't have an external name.
  // In the future we can add postmortem metadata on V8 regarding existing
  // symbols, but for now we'll use an heuristic to find the stack in the
  // error object.
  return v8()->types()->kSymbolType != -1 ? "Symbol()" : "<non-string>";
}

inline int StackTrace::GetFrameCount() { return len_; }


#undef ACCESSOR

}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
