#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <lldb/API/SBExpressionOptions.h>

#include "src/llnode.h"
#include "src/llscan.h"
#include "src/llv8.h"

namespace llnode {

using namespace lldb;

v8::LLV8 llv8;

char** CommandBase::ParseInspectOptions(char** cmd,
                                        v8::Value::InspectOptions* options) {
  static struct option opts[] = {
      {"full-string", no_argument, nullptr, 'F'},
      {"string-length", required_argument, nullptr, 0x1001},
      {"array-length", required_argument, nullptr, 0x1002},
      {"print-map", no_argument, nullptr, 'm'},
      {"print-source", no_argument, nullptr, 's'},
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
    int arg = getopt_long(argc, args, "Fms", opts, nullptr);
    if (arg == -1) break;

    switch (arg) {
      case 'F':
        options->string_length = 0;
        break;
      case 'm':
        options->print_map = true;
        break;
      case 0x1001:
        options->string_length = strtol(optarg, nullptr, 10);
        break;
      case 0x1002:
        options->array_length = strtol(optarg, nullptr, 10);
        break;
      case 's':
        options->print_source = true;
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
  llv8.Load(target);

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
    result.Printf(frame == selected_frame ? "  * frame #%u: 0x%016llx %s\n"
                                          : "    frame #%u: 0x%016llx %s\n",
                  i, static_cast<unsigned long long int>(frame.GetPC()),
                  res.c_str());
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
  llv8.Load(target);

  v8::Value v8_value(&llv8, value.GetValueAsSigned());
  v8::Error err;
  std::string res = v8_value.Inspect(&inspect_options, err);
  if (err.Fail()) {
    result.SetError("Failed to evaluate expression");
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
  llv8.Load(target);
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
  v8::Error err;
  v8::JSFrame v8_frame(&llv8, static_cast<int64_t>(frame.GetFP()));

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

}  // namespace llnode

namespace lldb {

bool PluginInitialize(SBDebugger d) {
  SBCommandInterpreter interpreter = d.GetCommandInterpreter();

  SBCommand v8 = interpreter.AddMultiwordCommand("v8", "Node.js helpers");

  v8.AddCommand(
      "bt", new llnode::BacktraceCmd(),
      "Show a backtrace with node.js JavaScript functions and their args. "
      "An optional argument is accepted; if that argument is a number, it "
      "specifies the number of frames to display. Otherwise all frames will "
      "be dumped.\n\n"
      "Syntax: v8 bt [number]\n");
  interpreter.AddCommand("jsstack", new llnode::BacktraceCmd(),
                         "Alias for `v8 bt`");

  v8.AddCommand("print", new llnode::PrintCmd(false),
                "Print short description of the JavaScript value.\n\n"
                "Syntax: v8 print expr\n");

  v8.AddCommand(
      "inspect", new llnode::PrintCmd(true),
      "Print detailed description and contents of the JavaScript value.\n\n"
      "Possible flags (all optional):\n\n"
      " * -F, --full-string    - print whole string without adding ellipsis\n"
      " * -m, --print-map      - print object's map address\n"
      " * -s, --print-source   - print source code for function objects\n"
      " * --string-length num  - print maximum of `num` characters in string\n"
      " * --array-length num   - print maximum of `num` elements in array\n"
      "\n"
      "Syntax: v8 inspect [flags] expr\n");
  interpreter.AddCommand("jsprint", new llnode::PrintCmd(true),
                         "Alias for `v8 inspect`");

  SBCommand source =
      v8.AddMultiwordCommand("source", "Source code information");
  source.AddCommand("list", new llnode::ListCmd(),
                    "Print source lines around a selected JavaScript frame.\n\n"
                    "Syntax: v8 source list\n");
  interpreter.AddCommand("jssource", new llnode::ListCmd(),
                         "Alias for `v8 source list`");

  v8.AddCommand("findjsobjects", new llnode::FindObjectsCmd(),
                "List all object types and instance counts grouped by map and "
                "sorted by instance count.\n"
#ifndef LLDB_SBMemoryRegionInfoList_h_
                "Requires `LLNODE_RANGESFILE` environment variable to be set "
                "to a file containing memory ranges for the core file being "
                "debugged.\n"
                "There are scripts for generating this file on Linux and Mac "
                "in the scripts directory of the llnode repository."
#endif  // LLDB_SBMemoryRegionInfoList_h_
                );

  interpreter.AddCommand("findjsobjects", new llnode::FindObjectsCmd(),
                         "Alias for `v8 findjsobjects`");

  v8.AddCommand("findjsinstances", new llnode::FindInstancesCmd(),
                "List all objects which share the specified map.\n"
                "Accepts the same options as `v8 inspect`");

  interpreter.AddCommand("findjsinstances", new llnode::FindInstancesCmd(),
                         "List all objects which share the specified map.\n");

  v8.AddCommand("nodeinfo", new llnode::NodeInfoCmd(),
                "Print information about Node.js\n");

  v8.AddCommand(
      "findrefs", new llnode::FindReferencesCmd(),
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
