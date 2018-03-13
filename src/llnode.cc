#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <cinttypes>

#include <lldb/API/SBExpressionOptions.h>

#include "src/error.h"
#include "src/llnode.h"
#include "src/llscan.h"
#include "src/llv8.h"

namespace llnode {

using lldb::SBCommandInterpreter;
using lldb::SBCommandReturnObject;
using lldb::SBDebugger;
using lldb::SBError;
using lldb::SBExpressionOptions;
using lldb::SBFrame;
using lldb::SBStream;
using lldb::SBSymbol;
using lldb::SBTarget;
using lldb::SBThread;
using lldb::SBValue;
using lldb::eReturnStatusFailed;
using lldb::eReturnStatusSuccessFinishResult;

char** CommandBase::ParseInspectOptions(char** cmd,
                                        v8::Value::InspectOptions* options) {
  static struct option opts[] = {
      {"full-string", no_argument, nullptr, 'F'},
      {"string-length", required_argument, nullptr, 'l'},
      {"array-length", required_argument, nullptr, 'l'},
      {"length", required_argument, nullptr, 'l'},
      {"print-map", no_argument, nullptr, 'm'},
      {"print-source", no_argument, nullptr, 's'},
      {"verbose", no_argument, nullptr, 'v'},
      {"detailed", no_argument, nullptr, 'd'},
      {nullptr, 0, nullptr, 0}};

  int argc = 1;
  for (char** p = cmd; p != nullptr && *p != nullptr; p++) argc++;

  char* args[argc];

  // Make this look like a command line, we need a valid element at index 0
  // for getopt_long to use in its error messages.
  char name[] = "llnode";
  args[0] = name;
  for (int i = 0; i < argc - 1; i++) args[i + 1] = cmd[i];

  // Reset getopts.
  optind = 0;
  opterr = 1;
  do {
    int arg = getopt_long(argc, args, "Fmsdvl:", opts, nullptr);
    if (arg == -1) break;

    switch (arg) {
      case 'F':
        options->length = 0;
        break;
      case 'm':
        options->print_map = true;
        break;
      case 'l':
        options->length = strtol(optarg, nullptr, 10);
        break;
      case 's':
        options->print_source = true;
        break;
      case 'd':
      case 'v':
        options->detailed = true;
        break;
      default:
        continue;
    }
  } while (true);

  // Use the original cmd array for our return value.
  return &cmd[optind - 1];
}


bool BacktraceCmd::DoExecute(SBDebugger d, char** cmd,
                             SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  SBThread thread = target.GetProcess().GetSelectedThread();
  if (!thread.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  errno = 0;
  int number =
      (cmd != nullptr && *cmd != nullptr) ? strtol(*cmd, nullptr, 10) : -1;
  if ((number == 0 && errno == EINVAL) || (number < 0 && number != -1)) {
    result.SetError("Invalid number of frames");
    return false;
  }

  // Load V8 constants from postmortem data
  llv8_->Load(target);

  {
    SBStream desc;
    if (!thread.GetDescription(desc)) return false;
    result.Printf(" * %s", desc.GetData());
  }

  SBFrame selected_frame = thread.GetSelectedFrame();

  uint32_t num_frames = thread.GetNumFrames();
  if (number != -1) num_frames = number;
  for (uint32_t i = 0; i < num_frames; i++) {
    SBFrame frame = thread.GetFrameAtIndex(i);
    const char star = (frame == selected_frame ? '*' : ' ');
    const uint64_t pc = frame.GetPC();

    if (!frame.GetSymbol().IsValid()) {
      Error err;
      v8::JSFrame v8_frame(llv8_, static_cast<int64_t>(frame.GetFP()));
      std::string res = v8_frame.Inspect(true, err);
      if (err.Success()) {
        result.Printf("  %c frame #%u: 0x%016" PRIx64 " %s\n", star, i, pc,
                      res.c_str());
        continue;
      } else {
        Error::PrintInDebugMode("%s", err.GetMessage());
      }
    }

#ifdef LLDB_SBMemoryRegionInfoList_h_
    // Heuristic: a PC in WX memory is almost certainly a V8 builtin.
    // TODO(bnoordhuis) Find a way to map the PC to the builtin's name.
    {
      lldb::SBMemoryRegionInfo info;
      if (target.GetProcess().GetMemoryRegionInfo(pc, info).Success() &&
          info.IsExecutable() && info.IsWritable()) {
        result.Printf("  %c frame #%u: 0x%016" PRIx64 " <builtin>\n", star, i,
                      pc);
        continue;
      }
    }
#endif  // LLDB_SBMemoryRegionInfoList_h_

    // C++ stack frame.
    SBStream desc;
    if (frame.GetDescription(desc))
      result.Printf("  %c %s", star, desc.GetData());
  }

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


bool PrintCmd::DoExecute(SBDebugger d, char** cmd,
                         SBCommandReturnObject& result) {
  if (cmd == nullptr || *cmd == nullptr) {
    if (detailed_) {
      result.SetError("USAGE: v8 inspect [flags] expr\n");
    } else {
      result.SetError("USAGE: v8 print expr\n");
    }
    return false;
  }

  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  v8::Value::InspectOptions inspect_options;

  inspect_options.detailed = detailed_;

  char** start = ParseInspectOptions(cmd, &inspect_options);

  std::string full_cmd;
  for (; start != nullptr && *start != nullptr; start++) full_cmd += *start;

  SBExpressionOptions options;
  SBValue value = target.EvaluateExpression(full_cmd.c_str(), options);
  if (value.GetError().Fail()) {
    SBStream desc;
    if (value.GetError().GetDescription(desc)) {
      result.SetError(desc.GetData());
    }
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  // Load V8 constants from postmortem data
  llv8_->Load(target);

  v8::Value v8_value(llv8_, value.GetValueAsSigned());
  Error err;
  std::string res = v8_value.Inspect(&inspect_options, err);
  if (err.Fail()) {
    result.SetError(err.GetMessage());
    return false;
  }

  result.Printf("%s\n", res.c_str());
  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


bool ListCmd::DoExecute(SBDebugger d, char** cmd,
                        SBCommandReturnObject& result) {
  if (cmd == nullptr || *cmd == nullptr) {
    result.SetError("USAGE: v8 source list\n");
    return false;
  }

  static SBFrame last_frame;
  static uint64_t last_line = 0;
  SBTarget target = d.GetSelectedTarget();
  SBThread thread = target.GetProcess().GetSelectedThread();
  if (!thread.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  std::string full_cmd;
  bool grab_line = false;
  bool line_switch = false;
  int line_from_switch = 0;
  for (char** start = cmd; *start != nullptr; start++) {
    if (grab_line) {
      grab_line = false;
      line_switch = true;
      errno = 0;
      line_from_switch = strtol(*start, nullptr, 10);
      if (errno) {
        result.SetError("Invalid line number");
        return false;
      }
      line_from_switch--;
    }
    if (strcmp(*start, "-l") == 0) {
      grab_line = true;
    }
    full_cmd += *start;
  }
  if (grab_line || (line_switch && line_from_switch < 0)) {
    result.SetError("Expected line number after -l");
    return false;
  }

  // Load V8 constants from postmortem data
  llv8_->Load(target);
  SBFrame frame = thread.GetSelectedFrame();
  SBSymbol symbol = frame.GetSymbol();

  bool reset_line = false;
  if (line_switch) {
    reset_line = true;
    last_line = line_from_switch;
  } else if (frame != last_frame) {
    last_line = 0;
    reset_line = true;
  }
  last_frame = frame;
  // C++ symbol
  if (symbol.IsValid()) {
    SBCommandInterpreter interpreter = d.GetCommandInterpreter();
    std::string cmd = "source list ";
    cmd += full_cmd;
    interpreter.HandleCommand(cmd.c_str(), result, false);
    return true;
  }

  // V8 frame
  Error err;
  v8::JSFrame v8_frame(llv8_, static_cast<int64_t>(frame.GetFP()));

  const static uint32_t kDisplayLines = 4;
  std::string* lines = new std::string[kDisplayLines];
  uint32_t lines_found = 0;

  uint32_t line_cursor = v8_frame.GetSourceForDisplay(
      reset_line, last_line, kDisplayLines, lines, lines_found, err);
  if (err.Fail()) {
    result.SetError(err.GetMessage());
    return false;
  }
  last_line = line_cursor;

  for (uint32_t i = 0; i < lines_found; i++) {
    result.Printf("  %d %s\n", line_cursor - lines_found + i + 1,
                  lines[i].c_str());
  }
  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


void InitDebugMode() {
  bool is_debug_mode = false;
  char* var = getenv("LLNODE_DEBUG");
  if (var != nullptr && strlen(var) != 0) {
    is_debug_mode = true;
  }

  Error::SetDebugMode(is_debug_mode);
}

}  // namespace llnode

namespace lldb {

bool PluginInitialize(SBDebugger d) {
  llnode::InitDebugMode();

  static llnode::v8::LLV8 llv8;
  static llnode::LLScan llscan = llnode::LLScan(&llv8);

  SBCommandInterpreter interpreter = d.GetCommandInterpreter();

  SBCommand v8 = interpreter.AddMultiwordCommand("v8", "Node.js helpers");

  v8.AddCommand(
      "bt", new llnode::BacktraceCmd(&llv8),
      "Show a backtrace with node.js JavaScript functions and their args. "
      "An optional argument is accepted; if that argument is a number, it "
      "specifies the number of frames to display. Otherwise all frames will "
      "be dumped.\n\n"
      "Syntax: v8 bt [number]\n");
  interpreter.AddCommand("jsstack", new llnode::BacktraceCmd(&llv8),
                         "Alias for `v8 bt`");

  v8.AddCommand("print", new llnode::PrintCmd(&llv8, false),
                "Print short description of the JavaScript value.\n\n"
                "Syntax: v8 print expr\n");

  v8.AddCommand(
      "inspect", new llnode::PrintCmd(&llv8, true),
      "Print detailed description and contents of the JavaScript value.\n\n"
      "Possible flags (all optional):\n\n"
      " * -F, --full-string    - print whole string without adding ellipsis\n"
      " * -m, --print-map      - print object's map address\n"
      " * -s, --print-source   - print source code for function objects\n"
      " * -l num, --length num - print maximum of `num` elements from "
      "string/array\n"
      "\n"
      "Syntax: v8 inspect [flags] expr\n");
  interpreter.AddCommand("jsprint", new llnode::PrintCmd(&llv8, true),
                         "Alias for `v8 inspect`");

  SBCommand source =
      v8.AddMultiwordCommand("source", "Source code information");
  source.AddCommand("list", new llnode::ListCmd(&llv8),
                    "Print source lines around a selected JavaScript frame.\n\n"
                    "Syntax: v8 source list\n");
  interpreter.AddCommand("jssource", new llnode::ListCmd(&llv8),
                         "Alias for `v8 source list`");

  v8.AddCommand("findjsobjects", new llnode::FindObjectsCmd(&llscan),
                "List all object types and instance counts grouped by type "
                "name and sorted by instance count. Use -d or --detailed to "
                "get an output grouped by type name, properties, and array "
                "length, as well as more information regarding each type.\n"
#ifndef LLDB_SBMemoryRegionInfoList_h_
                "Requires `LLNODE_RANGESFILE` environment variable to be set "
                "to a file containing memory ranges for the core file being "
                "debugged.\n"
                "There are scripts for generating this file on Linux and Mac "
                "in the scripts directory of the llnode repository."
#endif  // LLDB_SBMemoryRegionInfoList_h_
  );

  interpreter.AddCommand("findjsobjects", new llnode::FindObjectsCmd(&llscan),
                         "Alias for `v8 findjsobjects`");

  v8.AddCommand("findjsinstances", new llnode::FindInstancesCmd(&llscan, false),
                "List every object with the specified type name.\n"
                "Use -v or --verbose to display detailed `v8 inspect` output "
                "for each object.\n"
                "Accepts the same options as `v8 inspect`");

  interpreter.AddCommand("findjsinstances",
                         new llnode::FindInstancesCmd(&llscan, false),
                         "List all objects which share the specified map.\n");

  v8.AddCommand("nodeinfo", new llnode::NodeInfoCmd(&llscan),
                "Print information about Node.js\n");

  v8.AddCommand(
      "findrefs", new llnode::FindReferencesCmd(&llscan),
      "Finds all the object properties which meet the search criteria.\n"
      "The default is to list all the object properties that reference the "
      "specified value.\n"
      "Flags:\n\n"
      " * -v, --value expr     - all properties that refer to the specified "
      "JavaScript object (default)\n"
      " * -n, --name  name     - all properties with the specified name\n"
      " * -s, --string string  - all properties that refer to the specified "
      "JavaScript string value\n"
      "\n");

  return true;
}

}  // namespace lldb
