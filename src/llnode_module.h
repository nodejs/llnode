#ifndef SRC_LLNODE_MODULE_H
#define SRC_LLNODE_MODULE_H

#include <nan.h>
#include <memory>

namespace llnode {

class LLNodeApi;
class LLNodeHeapType;

class LLNode : public Nan::ObjectWrap {
  friend class LLNodeHeapType;

 public:
  static NAN_MODULE_INIT(Init);

  static ::v8::Persistent<::v8::Function> constructor;
  static ::v8::Persistent<::v8::FunctionTemplate> tpl;

 protected:
  std::unique_ptr<LLNodeApi> api;

 private:
  explicit LLNode();
  ~LLNode();

  static NAN_METHOD(New);
  static NAN_METHOD(FromCoreDump);
  // static NAN_METHOD(FromPid);
  // static NAN_METHOD(LoadDump);
  static NAN_METHOD(GetProcessInfo);
  static NAN_METHOD(GetProcessObject);
  static NAN_METHOD(GetHeapTypes);
  static NAN_METHOD(GetObjectAtAddress);
  bool _heap_initialized;
};

class LLNodeHeapType : public Nan::ObjectWrap {
  friend class LLNode;

 public:
  static NAN_MODULE_INIT(Init);
  explicit LLNodeHeapType(LLNode* llnode_ptr, size_t index);
  ::v8::Local<::v8::Object> Instantiate(::v8::Isolate* isolate,
                                        ::v8::Local<::v8::Context> context,
                                        ::v8::Local<::v8::Object> llnode_obj);

  static NAN_METHOD(NextInstance);

  static ::v8::Persistent<::v8::Function> constructor;
  static ::v8::Persistent<::v8::FunctionTemplate> tpl;

 private:
  ~LLNodeHeapType();
  static NAN_METHOD(New);

  void InitInstances();
  bool HasMoreInstances();

  // For getting objects
  LLNode* llnode_ptr;

  static ::v8::Persistent<::v8::Symbol> llnode_symbol;

  std::vector<uint64_t> type_instances;
  bool instances_initialized_;
  size_t current_instance_index;

  std::string type_name;
  size_t type_index;
  uint32_t type_ins_count;
  uint32_t type_total_size;
};

}  // namespace llnode

#endif
