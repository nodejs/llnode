#include <lldb/API/LLDB.h>
#include <set>

#include "src/llv8-inl.h"
#include "src/node-constants.h"

using lldb::SBFrame;
using lldb::SBProcess;
using lldb::SBStream;
using lldb::SBThread;

namespace llnode {
namespace node {
namespace constants {

void Environment::Load() {
  kReqWrapQueueOffset = LoadConstant(
      "offset_Environment__req_wrap_queue___Environment_ReqWrapQueue");
  kHandleWrapQueueOffset = LoadConstant(
      "offset_Environment__handle_wrap_queue___Environment_HandleWrapQueue");
  kEnvContextEmbedderDataIndex =
      LoadConstant("const_Environment__kContextEmbedderDataIndex__int");

  Error err;
  kCurrentEnvironment = LoadCurrentEnvironment(err);
}

addr_t Environment::LoadCurrentEnvironment(Error& err) {
  if (kEnvContextEmbedderDataIndex == -1) {
    err = Error::Failure("Missing Node's embedder data index");
    return 0;
  }
  addr_t currentEnvironment = 0;
  SBProcess process = target_.GetProcess();
  SBThread thread = process.GetSelectedThread();
  if (!thread.IsValid()) {
    err = Error::Failure("Invalid thread");
    return 0;
  }

  llv8()->Load(target_);

  SBStream desc;
  if (!thread.GetDescription(desc)) {
    err = Error::Failure("Couldn't get thread description");
    return 0;
  }
  SBFrame selected_frame = thread.GetSelectedFrame();

  uint32_t num_frames = thread.GetNumFrames();

  // Heuristically finds the native context and gets the Environment from its
  // embedder data.
  for (uint32_t i = 0; i < num_frames; i++) {
    SBFrame frame = thread.GetFrameAtIndex(i);

    if (!frame.GetSymbol().IsValid()) {
      Error v8_err;
      v8::JSFrame v8_frame(llv8(), static_cast<int64_t>(frame.GetFP()));
      v8::JSFunction v8_function = v8_frame.GetFunction(v8_err);
      if (v8_err.Fail()) {
        continue;
      }
      v8::Value val;
      val = v8_function.GetContext(v8_err);
      if (v8_err.Fail()) {
        continue;
      }
      bool found = false;
      std::set<uint64_t> visited_contexts;
      while (!found) {
        // Necessary to avoid an infinite loop.
        if (visited_contexts.count(val.raw())) {
          break;
        }
        visited_contexts.insert(val.raw());
        v8::Context context(val);
        v8::Value native = context.Native(v8_err);
        if (v8_err.Success()) {
          if (native.raw() == context.raw()) {
            found = true;
            currentEnvironment = CurrentEnvironmentFromContext(native, err);
            break;
          }
        }

        val = context.Previous(v8_err);
        if (v8_err.Fail()) {
          break;
        }
      }
      if (found) {
        break;
      }
    }
  }

  if (!currentEnvironment) {
    err =
        Error::Failure("Couldn't find the Environemnt from the native context");
  }

  return currentEnvironment;
}

addr_t Environment::CurrentEnvironmentFromContext(v8::Value context,
                                                  Error& err) {
  llv8()->Load(target_);

  v8::FixedArray contextArray = v8::FixedArray(context);
  v8::FixedArray embed = contextArray.Get<v8::FixedArray>(
      llv8()->context()->kEmbedderDataIndex, err);
  if (err.Fail()) {
    return 0;
  }
  v8::Smi encodedEnv = embed.Get<v8::Smi>(kEnvContextEmbedderDataIndex, err);
  if (err.Fail()) {
    return 0;
  }
  return encodedEnv.raw();
}


void ReqWrapQueue::Load() {
  kHeadOffset = LoadConstant(
      "offset_Environment_ReqWrapQueue__head___ListNode_ReqWrapQueue");
  kNextOffset = LoadConstant("offset_ListNode_ReqWrap__next___uintptr_t");
}

void ReqWrap::Load() {
  kListNodeOffset =
      LoadConstant("offset_ReqWrap__req_wrap_queue___ListNode_ReqWrapQueue");
}

void HandleWrapQueue::Load() {
  kHeadOffset = LoadConstant(
      "offset_Environment_HandleWrapQueue__head___ListNode_HandleWrap");
  kNextOffset = LoadConstant("offset_ListNode_HandleWrap__next___uintptr_t");
}

void HandleWrap::Load() {
  kListNodeOffset = LoadConstant(
      "offset_HandleWrap__handle_wrap_queue___ListNode_HandleWrap");
}

void BaseObject::Load() {
  kPersistentHandleOffset = LoadConstant(
      "offset_BaseObject__persistent_handle___v8_Persistent_v8_Object");
}
}  // namespace constants
}  // namespace node
}  // namespace llnode
