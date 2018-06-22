#include <nan.h>
#include "llnode_module.h"

namespace llnode {

NAN_MODULE_INIT(InitAll) {
  LLNode::Init(target);
  LLNodeHeapType::Init(target);
}

NODE_MODULE(addon, InitAll)

}  // namespace llnode
