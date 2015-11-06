#include "src/llnode.h"
#include "src/llv8.h"

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
  LLV8 llv8(target);

  {
    SBStream desc;
    if (!thread.GetDescription(desc)) return false;
    result.Printf(" * %s", desc.GetData());
  }

  SBFrame selected_frame = thread.GetSelectedFrame();

  uint32_t num_frames = thread.GetNumFrames();
  for (uint32_t i = 0; i < num_frames; i++) {
    SBFrame frame = thread.GetFrameAtIndex(i);
    SBSymbol symbol = frame.GetSymbol();

    // C++ symbol
    if (symbol.IsValid()) {
      SBStream desc;
      if (!frame.GetDescription(desc)) return false;
      result.Printf(frame == selected_frame ? "  * %s" : "    %s",
          desc.GetData());
      continue;
    }

    // V8 symbol
    result.Printf(
        frame == selected_frame ? "  * frame #%u: 0x%016llx " :
            "    frame #%u: 0x%016llx ",
        i, static_cast<unsigned long long int>(frame.GetPC()));

    result.Printf("%s\n", llv8.GetJSFrameName(frame.GetFP()).c_str());
  }

  return true;
}

}  // namespace llnode

namespace lldb {

bool PluginInitialize(SBDebugger d) {
  SBCommandInterpreter interpreter = d.GetCommandInterpreter();

  SBCommand v8 = interpreter.AddMultiwordCommand("v8", "Node.js helpers");

  v8.AddCommand("bt",
      new llnode::BacktraceCmd(),
      "Print node.js backtrace");

  return true;
}

}  // namespace lldb
