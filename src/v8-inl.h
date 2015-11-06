#ifndef SRC_V8_INL_H_
#define SRC_V8_INL_H_

#include "v8.h"

namespace llnode {

bool V8::is_smi(int64_t value) const {
  return (value & smi_.kTagMask) == smi_.kTag;
}


int64_t V8::untag_smi(int64_t value) const {
  return value >> (smi_.kShiftSize + smi_.kTagMask);
}


bool V8::is_heap_obj(int64_t value) const {
  return (value & heap_obj_.kTagMask) == heap_obj_.kTag;
}


inline int64_t V8::LoadPtr(lldb::addr_t addr, int64_t off) {
  return LoadPtr(static_cast<int64_t>(addr) + off);
}


inline int64_t V8::LeaHeapField(int64_t addr, int64_t off) {
  return addr - heap_obj_.kTag + off;
}


inline int64_t V8::LoadHeapField(int64_t addr, int64_t off) {
  return LoadPtr(LeaHeapField(addr, off));
}


inline int64_t V8::GetMap(int64_t addr) {
  return LoadHeapField(addr, heap_obj_.kMapOffset);
}

}  // namespace llnode

#endif  // SRC_V8_INL_H_
