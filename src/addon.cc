#include "llnode_module.h"
#include "napi.h"

namespace llnode {

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  Napi::Object new_exports = LLNode::Init(env, exports);
  return LLNodeHeapType::Init(env, new_exports);
  // return new_exports;
}

NODE_API_MODULE(addon, InitAll)

}  // namespace llnode
