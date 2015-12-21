#ifndef SRC_LLNODE_H_
#define SRC_LLNODE_H_

#include <string>

#include <lldb/API/LLDB.h>

#include "src/llv8.h"

namespace llnode {

class CommandBase : public lldb::SBCommandPluginInterface {
 public:
  char** ParseInspectOptions(char** cmd, v8::Value::InspectOptions* options);
};

class BacktraceCmd : public CommandBase {
 public:
  ~BacktraceCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class PrintCmd : public CommandBase {
 public:
  PrintCmd(bool detailed) : detailed_(detailed) {}

  ~PrintCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  bool detailed_;
};

class ListCmd : public CommandBase {
 public:
  ~ListCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

}  // namespace llnode

#endif  // SRC_LLNODE_H_
