#ifndef SRC_LLNODE_API_MODULE_H
#define SRC_LLNODE_API_MODULE_H

#include <napi.h>
#include <memory>

namespace llnode {

class LLNodeApi;
class LLNodeHeapType;

class LLNode : public Napi::ObjectWrap<LLNode> {
  friend class LLNodeHeapType;

 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static Napi::FunctionReference constructor;
  LLNode(const Napi::CallbackInfo& args);
  ~LLNode();

 private:
  LLNode() = delete;

  static Napi::Value FromCoreDump(const Napi::CallbackInfo& args);

  Napi::Value GetProcessInfo(const Napi::CallbackInfo& args);
  Napi::Value GetProcessObject(const Napi::CallbackInfo& args);
  Napi::Value GetHeapTypes(const Napi::CallbackInfo& args);
  Napi::Value GetObjectAtAddress(const Napi::CallbackInfo& args);

  bool heap_initialized_;

 protected:
  Napi::Object GetObjectAtAddress(Napi::Env env, uint64_t addr);
  std::unique_ptr<LLNodeApi> api_;
};

class LLNodeHeapType : public Napi::ObjectWrap<LLNodeHeapType> {
  friend class LLNode;

 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  LLNodeHeapType(const Napi::CallbackInfo& args);
  ~LLNodeHeapType();

  static Napi::Value NextInstance(const Napi::CallbackInfo& args);

  static Napi::FunctionReference constructor;

 private:
  LLNodeHeapType() = delete;

  void InitInstances();
  bool HasMoreInstances();
  LLNode* llnode();

  // For getting objects
  Napi::ObjectReference llnode_;

  std::vector<uint64_t> type_instances_;
  bool instances_initialized_;
  size_t current_instance_index_;

  std::string type_name_;
  size_t type_index_;
  uint32_t type_ins_count_;
  uint32_t type_total_size_;
};

}  // namespace llnode

#endif
