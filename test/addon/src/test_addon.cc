#include "nan.h"

namespace llnode_test_addon {

using v8::Handle;
using v8::Object;

NAN_METHOD(Method) {
  Nan::HandleScope scope;

  // Do nothing, this is just for debugging purposes
}

static void Init(Handle<Object> target) {
  Nan::SetMethod(target, "method", Method);
}

}  // namespace llnode_test_addon

NODE_MODULE(test_addon, llnode_test_addon::Init);
