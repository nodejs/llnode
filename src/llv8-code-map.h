#ifndef SRC_LLV8_CODE_MAP_H_
#define SRC_LLV8_CODE_MAP_H_

#include <string>

#include "src/llv8.h"

namespace llnode {
namespace v8 {

class CodeMap {
 public:
  CodeMap(LLV8* v8) : v8_(v8) {
  }

  std::string Collect(Error& err);

  inline LLV8* v8() const { return v8_; }

 private:
  static const int kMaxOldSpaceSearch = 0x2000;
  static const int kMaxPageCheck = 0x2000;

  int64_t FindOldSpace(int64_t heap, Error& err);
  std::string CollectArea(int64_t start, int64_t end, Error& err);
  std::string CollectObject(HeapObject obj, int64_t* size, Error& err);

  LLV8* v8_;
};

}  // namespace v8
}  // namespace llnode

#endif // SRC_LLV8_CODE_MAP_H_
