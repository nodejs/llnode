#ifndef SRC_LLV8_INL_H_
#define SRC_LLV8_INL_H_

#include "llv8.h"

namespace llnode {

bool LLV8::is_smi(int64_t value) const {
  return (value & smi_.kTagMask) == smi_.kTag;
}


int64_t LLV8::untag_smi(int64_t value) const {
  return value >> (smi_.kShiftSize + smi_.kTagMask);
}


bool LLV8::is_heap_obj(int64_t value) const {
  return (value & heap_obj_.kTagMask) == heap_obj_.kTag;
}


inline bool LLV8::LoadPtr(lldb::addr_t addr, int64_t off, int64_t* out) {
  return LoadPtr(static_cast<int64_t>(addr) + off, out);
}


inline int64_t LLV8::LeaHeapField(int64_t addr, int64_t off) {
  return addr - heap_obj_.kTag + off;
}


inline bool LLV8::LoadHeapField(int64_t addr, int64_t off, int64_t* out) {
  return LoadPtr(LeaHeapField(addr, off), out);
}


inline bool LLV8::GetMap(int64_t addr, int64_t* out) {
  return LoadHeapField(addr, heap_obj_.kMapOffset, out);
}

}  // namespace llnode

#endif  // SRC_LLV8_INL_H_
