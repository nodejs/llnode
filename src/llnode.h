#ifndef SRC_LLNODE_H_
#define SRC_LLNODE_H_

#include <string>

#include <lldb/API/LLDB.h>

#include "src/llv8.h"
#include "src/node.h"

namespace llnode {


// Use singleton pattern to avoid multiples instatiations
// of Settings
class Settings {
 private:
  static Settings instance;

  // Private constructors to prevent instantiations
  Settings(){};
  Settings(const Settings&){};
  ~Settings(){};

  // Override assignment operator to avoid copies
  Settings& operator=(const Settings&){};

  std::string color = "auto";

 public:
  static Settings* GetSettings();
  std::string SetColor(std::string option);
  std::string GetColor() { return color; };
  bool ShouldUseColor();
};

class CommandBase : public lldb::SBCommandPluginInterface {};

class BacktraceCmd : public CommandBase {
 public:
  BacktraceCmd(v8::LLV8* llv8) : llv8_(llv8) {}
  ~BacktraceCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  v8::LLV8* llv8_;
};

class SetPropertyColorCmd : public CommandBase {
 public:
  SetPropertyColorCmd(v8::LLV8* llv8) : llv8_(llv8) {}
  ~SetPropertyColorCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  v8::LLV8* llv8_;
};

class PrintCmd : public CommandBase {
 public:
  PrintCmd(v8::LLV8* llv8, bool detailed) : llv8_(llv8), detailed_(detailed) {}

  ~PrintCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  v8::LLV8* llv8_;
  bool detailed_;
};

class ListCmd : public CommandBase {
 public:
  ListCmd(v8::LLV8* llv8) : llv8_(llv8) {}
  ~ListCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  v8::LLV8* llv8_;
};

class WorkqueueCmd : public CommandBase {
 public:
  WorkqueueCmd(v8::LLV8* llv8, node::Node* node) : llv8_(llv8), node_(node) {}
  ~WorkqueueCmd() override {}

  inline v8::LLV8* llv8() { return llv8_; };
  inline node::Node* node() { return node_; };

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  virtual std::string GetResultMessage(node::Environment* env, Error& err) {
    return std::string();
  };

 private:
  v8::LLV8* llv8_;
  node::Node* node_;
};

class GetActiveHandlesCmd : public WorkqueueCmd {
 public:
  GetActiveHandlesCmd(v8::LLV8* llv8, node::Node* node)
      : WorkqueueCmd(llv8, node) {}

  std::string GetResultMessage(node::Environment* env, Error& err) override;
};

class GetActiveRequestsCmd : public WorkqueueCmd {
 public:
  GetActiveRequestsCmd(v8::LLV8* llv8, node::Node* node)
      : WorkqueueCmd(llv8, node) {}

  std::string GetResultMessage(node::Environment* env, Error& err) override;
};


}  // namespace llnode

#endif  // SRC_LLNODE_H_
