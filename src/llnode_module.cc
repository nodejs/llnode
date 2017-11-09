// Javascript module API for llnode/lldb
#include <cinttypes>
#include <cstdlib>

#include "src/llnode_api.h"
#include "src/llnode_module.h"

namespace llnode {

using ::v8::Array;
using ::v8::Context;
using ::v8::Exception;
using ::v8::Function;
using ::v8::FunctionCallback;
using ::v8::FunctionTemplate;
using ::v8::HandleScope;
using ::v8::Isolate;
using ::v8::Local;
using ::v8::Name;
using ::v8::Number;
using ::v8::Object;
using ::v8::Persistent;
using ::v8::PropertyAttribute;
using ::v8::PropertyCallbackInfo;
using ::v8::String;
using ::v8::Symbol;
using ::v8::Value;
using Nan::New;

Persistent<FunctionTemplate> LLNode::tpl;
Persistent<Function> LLNode::constructor;

template <typename T>
bool HasInstance(Local<Context> context, Local<Value> value) {
#if NODE_MODULE_VERSION < 57
  Local<FunctionTemplate> tpl = Nan::New(T::tpl);
  return tpl->HasInstance(value);
#else
  Local<Function> ctor = Nan::New(T::constructor);
  return value->InstanceOf(context, ctor).ToChecked();
#endif
}

NAN_MODULE_INIT(LLNode::Init) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<FunctionTemplate> local_tpl = Nan::New<FunctionTemplate>(New);
  tpl.Reset(isolate, local_tpl);
  Local<String> llnode_class = Nan::New("LLNode").ToLocalChecked();

  local_tpl->SetClassName(llnode_class);
  local_tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(local_tpl, "getProcessInfo", GetProcessInfo);
  Nan::SetPrototypeMethod(local_tpl, "getProcessObject", GetProcessObject);
  Nan::SetPrototypeMethod(local_tpl, "getHeapTypes", GetHeapTypes);
  Nan::SetPrototypeMethod(local_tpl, "getObjectAtAddress", GetObjectAtAddress);

  Local<Function> local_ctor = Nan::GetFunction(local_tpl).ToLocalChecked();
  constructor.Reset(isolate, local_ctor);

  Nan::Set(target, Nan::New("fromCoredump").ToLocalChecked(),
           Nan::New<FunctionTemplate>(FromCoreDump)->GetFunction());
  Nan::Set(target, llnode_class, local_ctor);
}

LLNode::LLNode() : api(new llnode::LLNodeApi()) {}

LLNode::~LLNode() {}

NAN_METHOD(LLNode::New) {}

NAN_METHOD(LLNode::FromCoreDump) {
  if (info.Length() < 2) {
    Nan::ThrowTypeError("Wrong number of args");
    return;
  }
  Nan::Utf8String filename(info[0]);
  Nan::Utf8String executable(info[1]);

  LLNode* obj = new LLNode();
  obj->_heap_initialized = false;
  bool ret = obj->api->Init(*filename, *executable);
  if (!ret) {
    Nan::ThrowTypeError("Failed to load coredump");
    return;
  }

  Local<Function> ctor = Nan::New(constructor);
  Local<Object> llnode_obj = Nan::NewInstance(ctor).ToLocalChecked();
  obj->Wrap(llnode_obj);
  info.GetReturnValue().Set(llnode_obj);
}

NAN_METHOD(LLNode::GetProcessInfo) {
  LLNode* obj = Nan::ObjectWrap::Unwrap<LLNode>(info.Holder());
  std::string process_info = obj->api->GetProcessInfo();
  info.GetReturnValue().Set(Nan::New(process_info).ToLocalChecked());
}

NAN_METHOD(LLNode::GetProcessObject) {
  LLNode* obj = Nan::ObjectWrap::Unwrap<LLNode>(info.Holder());
  uint32_t pid = obj->api->GetProcessID();
  std::string state = obj->api->GetProcessState();
  uint32_t thread_count = obj->api->GetThreadCount();

  Local<Object> result = Nan::New<Object>();
  result->Set(Nan::New("pid").ToLocalChecked(), Nan::New(pid));
  result->Set(Nan::New("state").ToLocalChecked(),
              Nan::New(state).ToLocalChecked());
  result->Set(Nan::New("threadCount").ToLocalChecked(), Nan::New(thread_count));

  Local<Array> thread_list = Nan::New<Array>();
  for (size_t i = 0; i < thread_count; i++) {
    Local<Object> thread = Nan::New<Object>();
    thread->Set(Nan::New("threadId").ToLocalChecked(), Nan::New<Number>(i));
    uint32_t frame_count = obj->api->GetFrameCount(i);
    thread->Set(Nan::New("frameCount").ToLocalChecked(), Nan::New(frame_count));
    Local<Array> frame_list = Nan::New<Array>();
    for (size_t j = 0; j < frame_count; j++) {
      Local<Object> frame = Nan::New<Object>();
      std::string frame_str = obj->api->GetFrame(i, j);
      frame->Set(Nan::New("function").ToLocalChecked(),
                 Nan::New(frame_str).ToLocalChecked());
      frame_list->Set(j, frame);
    }

    thread->Set(Nan::New("frames").ToLocalChecked(), frame_list);
    thread_list->Set(i, thread);
  }
  result->Set(Nan::New("threads").ToLocalChecked(), thread_list);

  info.GetReturnValue().Set(result);
}

NAN_METHOD(LLNode::GetHeapTypes) {
  LLNode* obj = Nan::ObjectWrap::Unwrap<LLNode>(info.Holder());
  Local<Object> llnode_obj = info.This();

  // Initialize the heap and the type iterators
  if (!obj->_heap_initialized) {
    obj->api->ScanHeap();
    obj->_heap_initialized = true;
  }

  uint32_t type_count = obj->api->GetTypeCount();
  Local<Array> type_list = Nan::New<Array>();
  Local<Function> ctor = Nan::New(LLNodeHeapType::constructor);
  for (size_t i = 0; i < type_count; i++) {
    Local<Value> argv[2] = {llnode_obj, Nan::New(static_cast<uint32_t>(i))};

    Local<Object> type_obj = Nan::NewInstance(ctor, 2, argv).ToLocalChecked();
    type_list->Set(i, type_obj);
  }

  info.GetReturnValue().Set(type_list);
}

Local<Object> GetTypeInstanceObject(Isolate* isolate, LLNodeApi* api,
                                    uint64_t addr) {
  HandleScope scope(isolate);

  Local<Object> result = Nan::New<Object>();
  char buf[20];
  snprintf(buf, sizeof(buf), "0x%016" PRIx64, addr);
  result->Set(Nan::New("address").ToLocalChecked(),
              Nan::New(buf).ToLocalChecked());

  std::string value = api->GetObject(addr);
  result->Set(Nan::New("value").ToLocalChecked(),
              Nan::New(value).ToLocalChecked());

  return result;
}

// TODO: create JS object to introspect core dump
// process/threads/frames
NAN_METHOD(LLNode::GetObjectAtAddress) {
  LLNode* obj = Nan::ObjectWrap::Unwrap<LLNode>(info.Holder());
  Nan::Utf8String address_str(info[0]);
  if ((*address_str)[0] != '0' || (*address_str)[1] != 'x' ||
      address_str.length() > 18) {
    Nan::ThrowTypeError("Invalid address");
    return;
  }
  uint64_t addr = std::strtoull(*address_str, nullptr, 16);
  Isolate* isolate = Isolate::GetCurrent();
  Local<Object> result = GetTypeInstanceObject(isolate, obj->api.get(), addr);
  info.GetReturnValue().Set(result);
}

Persistent<FunctionTemplate> LLNodeHeapType::tpl;
Persistent<Function> LLNodeHeapType::constructor;

Persistent<Symbol> LLNodeHeapType::llnode_symbol;

NAN_MODULE_INIT(LLNodeHeapType::Init) {
  Isolate* isolate = Isolate::GetCurrent();

  Local<FunctionTemplate> local_tpl = Nan::New<FunctionTemplate>(New);
  tpl.Reset(isolate, local_tpl);

  Local<String> type_class = Nan::New("LLNodeHeapType").ToLocalChecked();
  local_tpl->SetClassName(type_class);
  local_tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Local<Function> local_ctor = Nan::GetFunction(local_tpl).ToLocalChecked();
  constructor.Reset(isolate, local_ctor);
  llnode_symbol.Reset(
      isolate,
      Symbol::New(isolate, Nan::New("llnode_symbol").ToLocalChecked()));

  Nan::Set(target, Nan::New("nextInstance").ToLocalChecked(),
           Nan::New<FunctionTemplate>(NextInstance)->GetFunction());
  Nan::Set(target, type_class, local_ctor);
}

LLNodeHeapType::~LLNodeHeapType() {}
LLNodeHeapType::LLNodeHeapType(LLNode* ptr, size_t index) {
  llnode_ptr = ptr;
  instances_initialized_ = false;

  type_name = llnode_ptr->api->GetTypeName(index);
  type_index = index;
  type_ins_count = llnode_ptr->api->GetTypeInstanceCount(index);
  type_total_size = llnode_ptr->api->GetTypeTotalSize(index);
}

NAN_METHOD(LLNodeHeapType::New) {
  if (!info.IsConstructCall()) {
    int argc = info.Length();
    Local<Value> argv[argc];
    for (int i = 0; i < argc; ++i) {
      argv[i] = info[i];
    }

    Local<Function> ctor = Nan::New(constructor);
    info.GetReturnValue().Set(
        Nan::NewInstance(ctor, argc, argv).ToLocalChecked());
    return;
  }

  Local<Context> context = Nan::GetCurrentContext();
  if (info.Length() < 2 || !HasInstance<LLNode>(context, info[0])) {
    Nan::ThrowTypeError("Must pass an LLNode instance");
    return;
  }

  Local<Object> llnode_obj = Nan::To<Object>(info[0]).ToLocalChecked();
  LLNode* llnode_ptr = Nan::ObjectWrap::Unwrap<LLNode>(llnode_obj);
  uint32_t index = Nan::To<uint32_t>(info[1]).FromJust();

  LLNodeHeapType* obj = new LLNodeHeapType(llnode_ptr, index);
  Local<Object> instance = info.This();

  Local<String> typeName = Nan::New("typeName").ToLocalChecked();
  Local<String> instanceCount = Nan::New("instanceCount").ToLocalChecked();
  Local<String> totalSize = Nan::New("totalSize").ToLocalChecked();

  Local<Symbol> local_sym = Nan::New(llnode_symbol);
  instance->Set(typeName, Nan::New(obj->type_name).ToLocalChecked());
  instance->Set(instanceCount, Nan::New(obj->type_ins_count));
  instance->Set(totalSize, Nan::New(obj->type_total_size));
  instance->Set(local_sym, llnode_obj);

  obj->Wrap(instance);
  info.GetReturnValue().Set(instance);
}

void LLNodeHeapType::InitInstances() {
  std::set<uint64_t>* instances_set =
      llnode_ptr->api->GetTypeInstances(type_index);
  current_instance_index = 0;
  for (const uint64_t& addr : *instances_set) {
    type_instances.push_back(addr);
  }
  type_ins_count = type_instances.size();
  instances_initialized_ = true;
}

bool LLNodeHeapType::HasMoreInstances() {
  return current_instance_index < type_ins_count;
}

NAN_METHOD(LLNodeHeapType::NextInstance) {
  Local<Context> context = Nan::GetCurrentContext();
  if (info.Length() < 1 || !HasInstance<LLNodeHeapType>(context, info[0])) {
    Nan::ThrowTypeError("Must pass an LLNodeHeapType instance");
    return;
  }

  Local<Object> type_obj = Nan::To<Object>(info[0]).ToLocalChecked();
  LLNodeHeapType* obj = Nan::ObjectWrap::Unwrap<LLNodeHeapType>(type_obj);

  if (!obj->instances_initialized_) {
    obj->InitInstances();
  }

  if (!obj->HasMoreInstances()) {
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }

  uint64_t addr = obj->type_instances[obj->current_instance_index++];
  Isolate* isolate = Isolate::GetCurrent();
  Local<Object> result =
      GetTypeInstanceObject(isolate, obj->llnode_ptr->api.get(), addr);
  info.GetReturnValue().Set(result);
}

}  // namespace llnode
