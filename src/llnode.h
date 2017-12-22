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

class V8SnapshotCmd : public CommandBase {
 public:
  ~V8SnapshotCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  bool SerializeImpl(v8::Error &err);

  void SerializeSnapshot(v8::Error &err);

  void SerializeNodes(v8::Error &err);
  void SerializeNode(v8::Error &err, bool first_node);

  void SerializeEdges(v8::Error &err);
  void SerializeEdge(v8::Error &err, bool first_edge);

  // TODO
  void SerializeTraceNodeInfos(v8::Error &err);

  // TODO
  void SerializeTraceTree(v8::Error &err);
  void SerializeTraceNode(v8::Error &err);

  // TODO
  void SerializeSamples(v8::Error &err);

  // TODO
  void SerializeStrings(v8::Error &err);
  void SerializeString(v8::Error &err, const char* s);

 private:
  std::ofstream writer_;
};

}  // namespace llnode

#endif  // SRC_LLNODE_H_
