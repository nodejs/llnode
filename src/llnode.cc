#include "src/llnode.h"
#include "src/v8.h"

namespace llnode {

using namespace lldb;

BacktraceCmd::~BacktraceCmd() {
}

bool BacktraceCmd::DoExecute(SBDebugger d, char** cmd,
                             SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  SBThread thread = target.GetProcess().GetSelectedThread();
  if (!thread.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  // Load V8 constants from postmortem data
  V8 v8(target);

  result.Printf(" * thread #%u: tid 0x%04x\n", thread.GetIndexID(),
      static_cast<unsigned int>(thread.GetThreadID()));

  SBFrame selected_frame = thread.GetSelectedFrame();

  uint32_t num_frames = thread.GetNumFrames();
  for (uint32_t i = 0; i < num_frames; i++) {
    SBFrame frame = thread.GetFrameAtIndex(i);
    result.Printf(
        frame == selected_frame ? "  * frame #%u: 0x%016llx " :
            "    frame #%u: 0x%016llx ",
        i, static_cast<unsigned long long int>(frame.GetPC()));

    SBSymbol symbol = frame.GetSymbol();
    if (!symbol.IsValid()) {
      result.Printf("%s\n", v8.GetJSFrameName(frame.GetFP()).c_str());
      continue;
    }

    // C++ Symbol
    SBLineEntry line_entry = frame.GetLineEntry();
    SBFileSpec file_spec = line_entry.GetFileSpec();
    if (!file_spec.IsValid()) {
      result.Printf("%s\n", symbol.GetName());
      continue;
    }

    result.Printf("%s at %s:%d\n", symbol.GetName(), file_spec.GetFilename(),
        line_entry.GetLine());
  }

  return true;
}

}  // namespace llnode

namespace lldb {

bool PluginInitialize(lldb::SBDebugger d) {
  lldb::SBCommandInterpreter interpreter = d.GetCommandInterpreter();

  interpreter.AddCommand("v8bt",
      new llnode::BacktraceCmd(),
      "Print node.js backtrace");

  return true;
}

}  // namespace lldb
