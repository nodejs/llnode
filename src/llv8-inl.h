#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

#include "llv8.h"

namespace llnode {
namespace v8 {

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


bool Smi::Check() const {
  return (raw() & v8()->smi_.kTagMask) == v8()->smi_.kTag;
}


int64_t Smi::GetValue() const {
  return raw() >> (v8()->smi_.kShiftSize + v8()->smi_.kTagMask);
}


bool HeapObject::Check() const {
  return (raw() & v8()->heap_obj_.kTagMask) == v8()->heap_obj_.kTag;
}


int64_t HeapObject::LeaField(int64_t off) const {
  return raw() - v8()->heap_obj_.kTag + off;
}


int64_t HeapObject::LoadField(int64_t off, Error& err) {
  return v8()->LoadPtr(LeaField(off), err);
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
  int64_t type = LoadField(v8()->map_.kInstanceAttrsOffset, err);
  if (err.Fail()) return -1;
  return type & 0xff;
}


#define ACCESSOR(CLASS, METHOD, OFF, TYPE)                                    \
    TYPE CLASS::METHOD(Error& err) {                                          \
      return LoadFieldValue<TYPE>(v8()->OFF, err);                            \
    }


ACCESSOR(HeapObject, GetMap, heap_obj_.kMapOffset, HeapObject)

int64_t String::Representation(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string_.kRepresentationMask;
}


int64_t String::Encoding(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return -1;
  return type & v8()->string_.kEncodingMask;
}

ACCESSOR(String, Length, string_.kLengthOffset, Smi)

ACCESSOR(Script, Name, script_.kNameOffset, String)
ACCESSOR(Script, LineOffset, script_.kLineOffsetOffset, Smi)
ACCESSOR(Script, Source, script_.kSourceOffset, HeapObject)
ACCESSOR(Script, LineEnds, script_.kLineEndsOffset, HeapObject)

ACCESSOR(SharedFunctionInfo, Name, shared_info_.kNameOffset, String)
ACCESSOR(SharedFunctionInfo, InferredName, shared_info_.kInferredNameOffset,
         String)
ACCESSOR(SharedFunctionInfo, GetScript, shared_info_.kScriptOffset, Script)

int64_t SharedFunctionInfo::StartPosition(Error& err) {
  int64_t field = LoadField(v8()->shared_info_.kStartPositionOffset, err);
  if (err.Fail()) return -1;

  field &= 0xffffffff;
  field >>= v8()->shared_info_.kStartPositionShift;
  return field;
}

ACCESSOR(JSFunction, Info, js_function_.kSharedInfoOffset, SharedFunctionInfo);

ACCESSOR(ConsString, First, cons_string_.kFirstOffset, String);
ACCESSOR(ConsString, Second, cons_string_.kSecondOffset, String);

ACCESSOR(SlicedString, Parent, sliced_string_.kParentOffset, String);
ACCESSOR(SlicedString, Offset, sliced_string_.kOffsetOffset, Smi);

ACCESSOR(FixedArrayBase, Length, fixed_array_base_.kLengthOffset, Smi);

std::string String::GetValue(Error& err) {
  int64_t repr = Representation(err);
  if (err.Fail()) return std::string();

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (repr == v8()->string_.kSeqStringTag) {
    if (encoding == v8()->string_.kOneByteStringTag) {
      OneByteString one(this);
      return one.GetValue(err);
    } else if (encoding == v8()->string_.kTwoByteStringTag) {
      TwoByteString two(this);
      return two.GetValue(err);
    }

    err = Error::Failure("Unsupported seq string encoding");
    return std::string();
  }

  if (repr == v8()->string_.kConsStringTag) {
    ConsString cons(this);
    return cons.GetValue(err);
  }

  if (repr == v8()->string_.kSlicedStringTag) {
    SlicedString sliced(this);
    return sliced.GetValue(err);
  }

  err = Error::Failure("Unsupported string representation");
  return std::string();
}

std::string OneByteString::GetValue(Error& err) {
  int64_t chars = LeaField(v8()->one_byte_string_.kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadString(chars, len.GetValue(), err);
}

std::string TwoByteString::GetValue(Error& err) {
  int64_t chars = LeaField(v8()->two_byte_string_.kCharsOffset);
  Smi len = Length(err);
  if (err.Fail()) return std::string();
  return v8()->LoadTwoByteString(chars, len.GetValue(), err);
}

std::string ConsString::GetValue(Error& err) {
  String first = First(err);
  if (err.Fail()) return std::string();

  String second = Second(err);
  if (err.Fail()) return std::string();

  std::string tmp = first.GetValue(err);
  if (err.Fail()) return std::string();
  tmp += second.GetValue(err);
  if (err.Fail()) return std::string();

  return tmp;
}

std::string SlicedString::GetValue(Error& err) {
  String parent = Parent(err);
  if (err.Fail()) return std::string();

  Smi offset = Offset(err);
  if (err.Fail()) return std::string();

  Smi length = Length(err);
  if (err.Fail()) return std::string();

  std::string tmp = parent.GetValue(err);
  if (err.Fail()) return std::string();

  return tmp.substr(offset.GetValue(), length.GetValue());
}

int64_t FixedArray::LeaData() const {
  return LeaField(v8()->fixed_array_.kDataOffset);
}

template <class T>
T FixedArray::Get(int index, Error& err) {
  int64_t off = v8()->fixed_array_.kDataOffset + index * v8()->kPointerSize;
  return LoadFieldValue<T>(off, err);
}

#undef ACCESSOR

}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
