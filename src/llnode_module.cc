// Javascript module API for llnode/lldb
#include <cinttypes>
#include <cstdlib>

#include "src/llnode_api.h"
#include "src/llnode_module.h"

namespace llnode {

using Napi::Array;
using Napi::CallbackInfo;
using Napi::Function;
using Napi::FunctionReference;
using Napi::HandleScope;
using Napi::Number;
using Napi::Object;
using Napi::ObjectReference;
using Napi::Persistent;
using Napi::Reference;
using Napi::String;
using Napi::Symbol;
using Napi::TypeError;
using Napi::Value;

FunctionReference LLNode::constructor;

template <typename T>
bool HasInstance(Object obj) {
  return obj.InstanceOf(T::constructor.Value());
}

Object LLNode::Init(Napi::Env env, Object exports) {
  HandleScope scope(env);

  Function func = DefineClass(
      env, "LLNode",
      {
          InstanceMethod("getProcessInfo", &LLNode::GetProcessInfo),
          InstanceMethod("getProcessObject", &LLNode::GetProcessObject),
          InstanceMethod("getHeapTypes", &LLNode::GetHeapTypes),
          InstanceMethod("getObjectAtAddress", &LLNode::GetObjectAtAddress),
      });

  constructor = Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("fromCoredump",
              Function::New(env, LLNode::FromCoreDump, "fromCoredump"));

  exports.Set("LLNode", func);
  return exports;
}

LLNode::LLNode(const CallbackInfo& args)
    : ObjectWrap<LLNode>(args),
      heap_initialized_(false),
      api_(new llnode::LLNodeApi){};

LLNode::~LLNode() {}

Value LLNode::FromCoreDump(const CallbackInfo& args) {
  Napi::Env env = args.Env();

  if (!args[0].IsString() || !args[1].IsString()) {
    TypeError::New(env, "Must be called as fromCoreDump(filename, executable)")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object llnode_obj = constructor.New({});
  LLNode* obj = ObjectWrap<LLNode>::Unwrap(llnode_obj);
  obj->heap_initialized_ = false;
  std::string filename = args[0].As<String>();
  std::string executable = args[1].As<String>();
  bool ret = obj->api_->Init(filename.c_str(), executable.c_str());

  if (!ret) {
    TypeError::New(env, "Failed to load coredump").ThrowAsJavaScriptException();
    return env.Null();
  }

  return llnode_obj;
}

#define CHECK_INITIALIZED(api, env)                        \
  if (!api->IsInitialized()) {                             \
    TypeError::New(env, "LLNode has not been initialized") \
        .ThrowAsJavaScriptException();                     \
    return env.Null();                                     \
  }

Value LLNode::GetProcessInfo(const CallbackInfo& args) {
  CHECK_INITIALIZED(this->api_, args.Env())

  return String::New(args.Env(), this->api_->GetProcessInfo());
}

Value LLNode::GetProcessObject(const CallbackInfo& args) {
  Napi::Env env = args.Env();
  CHECK_INITIALIZED(this->api_, env)

  uint32_t pid = this->api_->GetProcessID();
  std::string state = this->api_->GetProcessState();
  uint32_t thread_count = this->api_->GetThreadCount();

  Object result = Object::New(env);

  result.Set(String::New(env, "pid"), Number::New(env, pid));
  result.Set(String::New(env, "state"), String::New(env, state));
  result.Set(String::New(env, "threadCount"), Number::New(env, thread_count));

  Array thread_list = Array::New(env);
  for (size_t i = 0; i < thread_count; i++) {
    Object thread = Object::New(env);
    thread.Set(String::New(env, "threadId"), Number::New(env, i));
    uint32_t frame_count = this->api_->GetFrameCount(i);
    thread.Set(String::New(env, "frameCount"), Number::New(env, frame_count));

    Array frame_list = Array::New(env);
    for (size_t j = 0; j < frame_count; j++) {
      Object frame = Object::New(env);
      std::string frame_str = this->api_->GetFrame(i, j);
      frame.Set(String::New(env, "function"), String::New(env, frame_str));
      frame_list.Set(j, frame);
    }

    thread.Set(String::New(env, "frames"), frame_list);
    thread_list.Set(i, thread);
  }

  result.Set(String::New(env, "threads"), thread_list);

  return result;
}

Value LLNode::GetHeapTypes(const CallbackInfo& args) {
  Napi::Env env = args.Env();
  CHECK_INITIALIZED(this->api_, env)
  Object llnode_obj = args.This().As<Object>();

  // Initialize the heap and the type iterators
  if (!this->heap_initialized_) {
    this->api_->ScanHeap();
    this->heap_initialized_ = true;
  }

  uint32_t type_count = this->api_->GetTypeCount();
  Array type_list = Array::New(env);
  for (size_t i = 0; i < type_count; i++) {
    Object type_obj = LLNodeHeapType::constructor.New(
        {llnode_obj, Number::New(env, static_cast<uint32_t>(i))});
    type_list.Set(i, type_obj);
  }

  return type_list;
}

Object LLNode::GetObjectAtAddress(Napi::Env env, uint64_t addr) {
  Object result = Object::New(env);

  char buf[20];
  snprintf(buf, sizeof(buf), "0x%016" PRIx64, addr);
  result.Set(String::New(env, "address"), String::New(env, buf));

  std::string value = this->api_->GetObject(addr);
  result.Set(String::New(env, "value"), String::New(env, value));

  return result;
}

// TODO: create JS object to introspect core dump
// process/threads/frames
Value LLNode::GetObjectAtAddress(const CallbackInfo& args) {
  Napi::Env env = args.Env();
  CHECK_INITIALIZED(this->api_, env)

  if (!args[0].IsString()) {
    TypeError::New(env, "First argument must be a string")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string address_str = args[0].As<String>();
  if (address_str[0] != '0' || address_str[1] != 'x' ||
      address_str.size() > 18) {
    TypeError::New(env, "Invalid address").ThrowAsJavaScriptException();
    return env.Null();
  }

  uint64_t addr = std::strtoull(address_str.c_str(), nullptr, 16);
  Object result = this->GetObjectAtAddress(args.Env(), addr);
  return result;
}

FunctionReference LLNodeHeapType::constructor;

Object LLNodeHeapType::Init(Napi::Env env, Object exports) {
  HandleScope scope(env);

  Function func = DefineClass(env, "LLNodeHeapType", {});

  constructor = Persistent(func);
  constructor.SuppressDestruct();

  exports.Set(String::New(env, "LLNodeHeapType"), func);
  exports.Set(String::New(env, "nextInstance"),
              Function::New(env, LLNodeHeapType::NextInstance, "nextInstance"));
  return exports;
}

LLNodeHeapType::~LLNodeHeapType() {}

LLNodeHeapType::LLNodeHeapType(const CallbackInfo& args)
    : ObjectWrap<LLNodeHeapType>(args) {
  Napi::Env env = args.Env();
  if (!args[0].IsObject() || !HasInstance<LLNode>(args[0].As<Object>())) {
    TypeError::New(env, "First argument must be a LLNode instance")
        .ThrowAsJavaScriptException();
    return;
  }

  if (!args[1].IsNumber()) {
    TypeError::New(env, "Second argument must be a number")
        .ThrowAsJavaScriptException();
    return;
  }

  Object llnode_obj = args[0].As<Object>();
  LLNode* llnode_ptr = ObjectWrap<LLNode>::Unwrap(llnode_obj);
  uint32_t index = args[1].As<Number>().Uint32Value();

  this->llnode_ = Persistent(llnode_obj);
  this->instances_initialized_ = false;
  this->current_instance_index_ = 0;

  this->type_name_ = llnode_ptr->api_->GetTypeName(index);
  this->type_index_ = index;
  this->type_ins_count_ = llnode_ptr->api_->GetTypeInstanceCount(index);
  this->type_total_size_ = llnode_ptr->api_->GetTypeTotalSize(index);

  Object instance = args.This().As<Object>();

  String typeName = String::New(env, "typeName");
  String instanceCount = String::New(env, "instanceCount");
  String totalSize = String::New(env, "totalSize");

  instance.Set(typeName, String::New(env, this->type_name_));
  instance.Set(instanceCount, Number::New(env, this->type_ins_count_));
  instance.Set(totalSize, Number::New(env, this->type_total_size_));
  instance.Set(String::New(env, "llnode"), llnode_obj);
}

LLNode* LLNodeHeapType::llnode() {
  return ObjectWrap<LLNode>::Unwrap(this->llnode_.Value());
}

void LLNodeHeapType::InitInstances() {
  auto instances_set =
      this->llnode()->api_->GetTypeInstances(this->type_index_);
  this->current_instance_index_ = 0;

  for (const uint64_t& addr : *instances_set) {
    this->type_instances_.push_back(addr);
  }

  this->type_ins_count_ = this->type_instances_.size();
  this->instances_initialized_ = true;
}

bool LLNodeHeapType::HasMoreInstances() {
  return current_instance_index_ < type_ins_count_;
}

Value LLNodeHeapType::NextInstance(const CallbackInfo& args) {
  Napi::Env env = args.Env();
  if (!args[0].IsObject() ||
      !HasInstance<LLNodeHeapType>(args[0].As<Object>())) {
    TypeError::New(env, "First argument must be a LLNoteHeapType instance")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  LLNodeHeapType* obj =
      ObjectWrap<LLNodeHeapType>::Unwrap(args[0].As<Object>());
  if (!obj->instances_initialized_) {
    obj->InitInstances();
  }

  if (!obj->HasMoreInstances()) {
    return env.Undefined();
  }

  uint64_t addr = obj->type_instances_[obj->current_instance_index_++];
  Object result = obj->llnode()->GetObjectAtAddress(env, addr);
  return result;
}

}  // namespace llnode
