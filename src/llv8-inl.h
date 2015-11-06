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

ACCESSOR(SharedFunctionInfo, Name, shared_info_.kNameOffset, String)
ACCESSOR(SharedFunctionInfo, InferredName, shared_info_.kInferredNameOffset,
         String)
ACCESSOR(SharedFunctionInfo, GetScript, shared_info_.kScriptOffset, Script)

int64_t SharedFunctionInfo::StartPosition(Error& err) {
  Smi field = LoadFieldValue<Smi>(v8()->shared_info_.kStartPositionOffset, err);
  if (err.Fail()) return -1;

  return field.GetValue() >> v8()->shared_info_.kStartPositionShift;
}

ACCESSOR(JSFunction, Info, js_function_.kSharedInfoOffset, SharedFunctionInfo);

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

ACCESSOR(ConsString, First, cons_string_.kFirstOffset, String);
ACCESSOR(ConsString, Second, cons_string_.kSecondOffset, String);

#undef ACCESSOR

}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
