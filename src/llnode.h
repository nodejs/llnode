#ifndef SRC_LLNODE_H_
#define SRC_LLNODE_H_

#include <string>

#include <lldb/API/LLDB.h>

namespace llnode {

class BacktraceCmd : public lldb::SBCommandPluginInterface {
 public:
  ~BacktraceCmd() override {
  }

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class PrintCmd : public lldb::SBCommandPluginInterface {
 public:
  PrintCmd(bool detailed) : detailed_(detailed) {
  }

  ~PrintCmd() override {
  }

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  bool detailed_;
};

class CodeMap : public lldb::SBCommandPluginInterface {
 public:
  ~CodeMap() override {
  }

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class ListCmd : public lldb::SBCommandPluginInterface {
 public:
  ~ListCmd() override {
  }

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

}  // namespace llnode

#endif  // SRC_LLNODE_H_
