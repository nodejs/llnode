#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

#include "llv8.h"

namespace llnode {
namespace v8 {

template <>
double LLV8::LoadValue<double>(int64_t addr, Error& err) {
  return LoadDouble(addr, err);
}

template <class T>
T LLV8::LoadValue(int64_t addr, Error& err) {
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


bool Smi::Check() const {
  return (raw() & v8()->smi()->kTagMask) == v8()->smi()->kTag;
}


int64_t Smi::GetValue() const {
  return raw() >> (v8()->smi()->kShiftSize + v8()->smi()->kTagMask);
}


bool HeapObject::Check() const {
  return (raw() & v8()->heap_obj()->kTagMask) == v8()->heap_obj()->kTag;
}


int64_t HeapObject::LeaField(int64_t off) const {
  return raw() - v8()->heap_obj()->kTag + off;
}


int64_t HeapObject::LoadField(int64_t off, Error& err) {
  return v8()->LoadPtr(LeaField(off), err);
}


template <>
double HeapObject::LoadFieldValue<double>(int64_t off, Error& err) {
  return v8()->LoadValue<double>(LeaField(off), err);
}


template <class T>
T HeapObject::LoadFieldValue(int64_t off, Error& err) {
  T res = v8()->LoadValue<T>(LeaField(off), err);
  if (err.Fail()) return T();
  if (!res.Check()) {
    err = Error::Failure("Invalid value");
    return T();
  }

  return res;
}


int64_t HeapObject::GetType(Error& err) {
  HeapObject obj = GetMap(err);
  if (err.Fail()) return -1;

  Map map(obj);
  return map.GetType(err);
}


int64_t Map::GetType(Error& err) {
  int64_t type = LoadField(v8()->map()->kInstanceAttrsOffset, err);
  if (err.Fail()) return -1;
  return type & 0xff;
}


int64_t JSFrame::LeaParamSlot(int slot, int count) const {
  return raw() + v8()->frame()->kArgsOffset +
      (count - slot - 1) * v8()->common()->kPointerSize;
}


Value JSFrame::GetReceiver(int count, Error &err) {
  return GetParam(-1, count, err);
}


Value JSFrame::GetParam(int slot, int count, Error& err) {
  int64_t addr = LeaParamSlot(slot, count);
  return v8()->LoadValue<Value>(addr, err);
}


std::string JSFunction::Name(Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return std::string();

  return Name(info, err);
}


bool Map::IsDictionary(Error& err) {
  int64_t field = BitField3(err);
  if (err.Fail()) return false;

  return (field & (1 << v8()->map()->kDictionaryMapShift)) != 0;
}


int64_t Map::NumberOfOwnDescriptors(Error& err) {
  int64_t field = BitField3(err);
  if (err.Fail()) return false;

  // Skip EnumLength
  field &= v8()->map()->kNumberOfOwnDescriptorsMask;
  field >>= v8()->map()->kNumberOfOwnDescriptorsShift;
  return field;
}


#define ACCESSOR(CLASS, METHOD, OFF, TYPE)                                    \
    TYPE CLASS::METHOD(Error& err) {                                          \
      return LoadFieldValue<TYPE>(v8()->OFF, err);                            \
    }


ACCESSOR(HeapObject, GetMap, heap_obj()->kMapOffset, HeapObject)

ACCESSOR(Map, MaybeConstructor, map()->kMaybeConstructorOffset, HeapObject)
ACCESSOR(Map, InstanceDescriptors, map()->kInstanceDescriptorsOffset, HeapObject)

int64_t Map::BitField3(Error& err) {
  return LoadField(v8()->map()->kBitField3Offset, err) & 0xffffffff;
}

int64_t Map::InObjectProperties(Error& err) {
  return LoadField(v8()->map()->kInObjectPropertiesOffset, err) & 0xff;
}

int64_t Map::InstanceSize(Error& err) {
  return (LoadField(v8()->map()->kInstanceSizeOffset, err) & 0xff) *
      v8()->common()->kPointerSize;
}

ACCESSOR(JSObject, Properties, js_object()->kPropertiesOffset, HeapObject)
ACCESSOR(JSObject, Elements, js_object()->kElementsOffset, HeapObject)

ACCESSOR(HeapNumber, GetValue, heap_number()->kValueOffset, double)

ACCESSOR(JSArray, Length, js_array()->kLengthOffset, Smi)

int64_t String::Representation(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string()->kRepresentationMask;
}


int64_t String::Encoding(Error& err) {
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

ACCESSOR(Oddball, Kind, oddball()->kKindOffset, Smi)

int64_t JSArrayBuffer::BackingStore(Error& err) {
  return LoadField(v8()->js_array_buffer()->kBackingStoreOffset, err);
}

int64_t JSArrayBuffer::BitField(Error& err) {
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
int64_t SharedFunctionInfo::ParameterCount(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kParameterCountOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  return field;
}

// TODO(indutny): this field is a Smi on 32bit
int64_t SharedFunctionInfo::StartPosition(Error& err) {
  int64_t field = LoadField(v8()->shared_info()->kStartPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  field >>= v8()->shared_info()->kStartPositionShift;
  return field;
}

ACCESSOR(JSFunction, Info, js_function()->kSharedInfoOffset, SharedFunctionInfo);

ACCESSOR(ConsString, First, cons_string()->kFirstOffset, String);
ACCESSOR(ConsString, Second, cons_string()->kSecondOffset, String);

ACCESSOR(SlicedString, Parent, sliced_string()->kParentOffset, String);
ACCESSOR(SlicedString, Offset, sliced_string()->kOffsetOffset, Smi);

ACCESSOR(FixedArrayBase, Length, fixed_array_base()->kLengthOffset, Smi);

std::string OneByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->one_byte_string()->kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadString(chars, len.GetValue(), err);
}

std::string TwoByteString::ToString(Error& err) {
  int64_t chars = LeaField(v8()->two_byte_string()->kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadTwoByteString(chars, len.GetValue(), err);
}

std::string ConsString::ToString(Error& err) {
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

std::string SlicedString::ToString(Error& err) {
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

int64_t FixedArray::LeaData() const {
  return LeaField(v8()->fixed_array()->kDataOffset);
}

template <class T>
T FixedArray::Get(int index, Error& err) {
  int64_t off = v8()->fixed_array()->kDataOffset +
      index * v8()->common()->kPointerSize;
  return LoadFieldValue<T>(off, err);
}

Smi DescriptorArray::GetDetails(int index, Error& err) {
  return Get<Smi>(v8()->descriptor_array()->kFirstIndex +
                      index * v8()->descriptor_array()->kSize +
                      v8()->descriptor_array()->kDetailsOffset,
                  err);
}

Value DescriptorArray::GetKey(int index, Error& err) {
  return Get<Value>(v8()->descriptor_array()->kFirstIndex +
                        index * v8()->descriptor_array()->kSize +
                        v8()->descriptor_array()->kKeyOffset,
                    err);
}

bool DescriptorArray::IsFieldDetails(Smi details) {
  return (details.GetValue() & v8()->descriptor_array()->kPropertyTypeMask) ==
      v8()->descriptor_array()->kFieldType;
}

bool DescriptorArray::IsDoubleField(Smi details) {
  int64_t repr = details.GetValue();
  repr &= v8()->descriptor_array()->kRepresentationMask;
  repr >>= v8()->descriptor_array()->kRepresentationShift;

  return repr == v8()->descriptor_array()->kRepresentationDouble;
}

int64_t DescriptorArray::FieldIndex(Smi details) {
  return (details.GetValue() & v8()->descriptor_array()->kPropertyIndexMask) >>
      v8()->descriptor_array()->kPropertyIndexShift;
}

Value NameDictionary::GetKey(int index, Error& err) {
  int64_t off = v8()->name_dictionary()->kPrefixSize +
      index * v8()->name_dictionary()->kEntrySize +
      v8()->name_dictionary()->kKeyOffset;
  return FixedArray::Get<Value>(off, err);
}

Value NameDictionary::GetValue(int index, Error& err) {
  int64_t off = v8()->name_dictionary()->kPrefixSize +
      index * v8()->name_dictionary()->kEntrySize +
      v8()->name_dictionary()->kValueOffset;
  return FixedArray::Get<Value>(off, err);
}

int64_t NameDictionary::Length(Error& err) {
  Smi length = FixedArray::Length(err);
  if (err.Fail()) return -1;

  int64_t res = length.GetValue() - v8()->name_dictionary()->kPrefixSize;
  res /= v8()->name_dictionary()->kEntrySize;
  return res;
}

bool Oddball::IsHoleOrUndefined(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return false;

  return kind.GetValue() == v8()->oddball()->kTheHole ||
         kind.GetValue() == v8()->oddball()->kUndefined;
}

bool Oddball::IsHole(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return false;

  return kind.GetValue() == v8()->oddball()->kTheHole;
}

bool JSArrayBuffer::WasNeutered(Error& err) {
  int64_t field = BitField(err);
  if (err.Fail()) return false;

  return (field & v8()->js_array_buffer()->kWasNeuteredMask) != 0;
}

#undef ACCESSOR

}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
