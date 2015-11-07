#include <lldb/API/SBExpressionOptions.h>

#include "src/llnode.h"
#include "src/llv8.h"

namespace llnode {

using namespace lldb;

v8::LLV8 llv8;

bool BacktraceCmd::DoExecute(SBDebugger d, char** cmd,
                             SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  SBThread thread = target.GetProcess().GetSelectedThread();
  if (!thread.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  // Load V8 constants from postmortem data
  llv8.Load(target);

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
      if (!frame.GetDescription(desc)) continue;
      result.Printf(frame == selected_frame ? "  * %s" : "    %s",
          desc.GetData());
      continue;
    }

    // V8 frame
    v8::Error err;
    v8::JSFrame v8_frame(&llv8, static_cast<int64_t>(frame.GetFP()));
    std::string res = v8_frame.Inspect(true, err);

    // Skip invalid frames
    if (err.Fail()) continue;

    // V8 symbol
    result.Printf(
        frame == selected_frame ? "  * frame #%u: 0x%016llx %s\n" :
            "    frame #%u: 0x%016llx %s\n",
        i, static_cast<unsigned long long int>(frame.GetPC()), res.c_str());
  }

  return true;
}


bool PrintCmd::DoExecute(SBDebugger d, char** cmd,
                         SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  std::string full_cmd;
  for (char** start = cmd; *start != nullptr; start++) {
    full_cmd += *start;
  }

  SBExpressionOptions options;
  SBValue value = target.EvaluateExpression(full_cmd.c_str(), options);
  if (value.GetError().Fail()) {
    SBStream desc;
    if (!value.GetError().GetDescription(desc)) return false;
    result.SetError(desc.GetData());
    return false;
  }

  // Load V8 constants from postmortem data
  llv8.Load(target);

  v8::Value v8_value(&llv8, value.GetValueAsSigned());
  v8::Error err;
  std::string res = v8_value.Inspect(detailed_, err);
  if (err.Fail()) {
    result.SetError("Failed to evaluate expression");
    return false;
  }

  result.Printf("%s\n", res.c_str());

  return true;
}

}  // namespace llnode

namespace lldb {

bool PluginInitialize(SBDebugger d) {
  SBCommandInterpreter interpreter = d.GetCommandInterpreter();

  SBCommand v8 = interpreter.AddMultiwordCommand("v8", "Node.js helpers");

  v8.AddCommand("bt", new llnode::BacktraceCmd(), "Print node.js backtrace");
  v8.AddCommand("print", new llnode::PrintCmd(false), "Print JavaScript value");
  v8.AddCommand("inspect", new llnode::PrintCmd(true),
      "Detailed inspection of JavaScript value");

  return true;
}

}  // namespace lldb
