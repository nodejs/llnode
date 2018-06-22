#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBCompileUnit.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBFileSpec.h>
#include <lldb/API/SBFrame.h>
#include <lldb/API/SBModule.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBSymbol.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>
#include <lldb/lldb-enumerations.h>

#include <algorithm>
#include <cstring>

#include "src/llnode_api.h"
#include "src/llscan.h"
#include "src/llv8.h"

namespace llnode {

LLNodeApi::LLNodeApi()
    : loaded(false),
      debugger(new lldb::SBDebugger()),
      target(new lldb::SBTarget()),
      process(new lldb::SBProcess()),
      llv8(new v8::LLV8()),
      llscan(new LLScan(llv8.get())) {}
LLNodeApi::~LLNodeApi() = default;
LLNodeApi::LLNodeApi(LLNodeApi&&) = default;
LLNodeApi& LLNodeApi::operator=(LLNodeApi&&) = default;

bool LLNodeApi::debugger_initialized_ = false;

/* Initialize the SB API and load the core dump */
bool LLNodeApi::Init(const char* filename, const char* executable) {
  if (!LLNodeApi::debugger_initialized_) {
    lldb::SBDebugger::Initialize();
    LLNodeApi::debugger_initialized_ = true;
  }

  if (loaded) {
    return false;
  }

  *debugger = lldb::SBDebugger::Create();
  loaded = true;

  // Single instance target for now
  *target = debugger->CreateTarget(executable);
  if (!target->IsValid()) {
    return false;
  }

  *process = target->LoadCore(filename);
  // Load V8 constants from postmortem data
  llscan->v8()->Load(*target);
  return true;
}

std::string LLNodeApi::GetProcessInfo() {
  lldb::SBStream info;
  process->GetDescription(info);
  return std::string(info.GetData());
}

uint32_t LLNodeApi::GetProcessID() { return process->GetProcessID(); }

std::string LLNodeApi::GetProcessState() {
  int state = process->GetState();

  switch (state) {
    case lldb::StateType::eStateInvalid:
      return "invalid";
    case lldb::StateType::eStateUnloaded:
      return "unloaded";
    case lldb::StateType::eStateConnected:
      return "connected";
    case lldb::StateType::eStateAttaching:
      return "attaching";
    case lldb::StateType::eStateLaunching:
      return "launching";
    case lldb::StateType::eStateStopped:
      return "stopped";
    case lldb::StateType::eStateRunning:
      return "running";
    case lldb::StateType::eStateStepping:
      return "stepping";
    case lldb::StateType::eStateCrashed:
      return "crashed";
    case lldb::StateType::eStateDetached:
      return "detached";
    case lldb::StateType::eStateExited:
      return "exited";
    case lldb::StateType::eStateSuspended:
      return "suspended";
    default:
      return "unknown";
  }
}

uint32_t LLNodeApi::GetThreadCount() { return process->GetNumThreads(); }

uint32_t LLNodeApi::GetFrameCount(size_t thread_index) {
  lldb::SBThread thread = process->GetThreadAtIndex(thread_index);
  if (!thread.IsValid()) {
    return 0;
  }
  return thread.GetNumFrames();
}

// TODO: should return a class with
// functionName, directory, file, complieUnitDirectory, compileUnitFile
std::string LLNodeApi::GetFrame(size_t thread_index, size_t frame_index) {
  lldb::SBThread thread = process->GetThreadAtIndex(thread_index);
  lldb::SBFrame frame = thread.GetFrameAtIndex(frame_index);
  lldb::SBSymbol symbol = frame.GetSymbol();

  std::string result;
  char buf[4096];
  if (symbol.IsValid()) {
    snprintf(buf, sizeof(buf), "Native: %s", frame.GetFunctionName());
    result += buf;

    lldb::SBModule module = frame.GetModule();
    lldb::SBFileSpec moduleFileSpec = module.GetFileSpec();
    snprintf(buf, sizeof(buf), " [%s/%s]", moduleFileSpec.GetDirectory(),
             moduleFileSpec.GetFilename());
    result += buf;

    lldb::SBCompileUnit compileUnit = frame.GetCompileUnit();
    lldb::SBFileSpec compileUnitFileSpec = compileUnit.GetFileSpec();
    if (compileUnitFileSpec.GetDirectory() != nullptr ||
        compileUnitFileSpec.GetFilename() != nullptr) {
      snprintf(buf, sizeof(buf), "\n\t [%s: %s]",
               compileUnitFileSpec.GetDirectory(),
               compileUnitFileSpec.GetFilename());
      result += buf;
    }
  } else {
    // V8 frame
    llnode::v8::Error err;
    llnode::v8::JSFrame v8_frame(llscan->v8(),
                                 static_cast<int64_t>(frame.GetFP()));
    std::string frame_str = v8_frame.Inspect(true, err);

    // Skip invalid frames
    if (err.Fail() || frame_str.length() == 0 || frame_str[0] == '<') {
      if (frame_str[0] == '<') {
        snprintf(buf, sizeof(buf), "Unknown: %s", frame_str.c_str());
        result += buf;
      } else {
        result += "???";
      }
    } else {
      // V8 symbol
      snprintf(buf, sizeof(buf), "JavaScript: %s", frame_str.c_str());
      result += buf;
    }
  }
  return result;
}

void LLNodeApi::ScanHeap() {
  lldb::SBCommandReturnObject result;
  // Initial scan to create the JavaScript object map
  // TODO: make it possible to create multiple instances
  // of llscan and llnode
  if (!llscan->ScanHeapForObjects(*target, result)) {
    return;
  }
  object_types.clear();

  // Load the object types into a vector
  for (const auto& kv : llscan->GetMapsToInstances()) {
    object_types.push_back(kv.second);
  }

  // Sort by instance count
  std::sort(object_types.begin(), object_types.end(),
            TypeRecord::CompareInstanceCounts);
}

uint32_t LLNodeApi::GetTypeCount() { return object_types.size(); }

std::string LLNodeApi::GetTypeName(size_t type_index) {
  if (object_types.size() <= type_index) {
    return "";
  }
  return object_types[type_index]->GetTypeName();
}

uint32_t LLNodeApi::GetTypeInstanceCount(size_t type_index) {
  if (object_types.size() <= type_index) {
    return 0;
  }
  return object_types[type_index]->GetInstanceCount();
}

uint32_t LLNodeApi::GetTypeTotalSize(size_t type_index) {
  if (object_types.size() <= type_index) {
    return 0;
  }
  return object_types[type_index]->GetTotalInstanceSize();
}

std::set<uint64_t>* LLNodeApi::GetTypeInstances(size_t type_index) {
  if (object_types.size() <= type_index) {
    return nullptr;
  }
  return &(object_types[type_index]->GetInstances());
}

std::string LLNodeApi::GetObject(uint64_t address) {
  v8::Value v8_value(llscan->v8(), address);
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = true;
  inspect_options.length = 16;

  v8::Error err;
  std::string result = v8_value.Inspect(&inspect_options, err);
  if (err.Fail()) {
    return "Failed to get object";
  }
  return result;
}
}  // namespace llnode
