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
  struct Node {
    enum Type {
    kHidden = 0,  // v8::HeapGraphNode::kHidden,
      kArray = 1,  // v8::HeapGraphNode::kArray,
      kString = 2,  // v8::HeapGraphNode::kString,
      kObject = 3,  // v8::HeapGraphNode::kObject,
      kCode = 4,  // v8::HeapGraphNode::kCode,
    kClosure = 5,  // v8::HeapGraphNode::kClosure,
      kRegExp = 6,  //v8::HeapGraphNode::kRegExp,
      kHeapNumber = 7,  // v8::HeapGraphNode::kHeapNumber,
    kNative = 8,  // v8::HeapGraphNode::kNative,
    kSynthetic = 9,  // v8::HeapGraphNode::kSynthetic,
    kConsString = 10,  // v8::HeapGraphNode::kConsString,
    kSlicedString = 11,  // v8::HeapGraphNode::kSlicedString,
    kSymbol = 12,  // v8::HeapGraphNode::kSymbol,
      kSimdValue = 13,  // v8::HeapGraphNode::kSimdValue

      kInvalid = -1
    };
    // static const int kNoEntry;

    uint64_t addr;
    Type type;
    uint64_t name_id;
    uint64_t node_id;
    uint64_t self_size;
    uint64_t children;
    uint64_t trace_node_id;
  };

  struct Edge {
    // TODO (mmarchini) this should come from V8 metadata
    enum Type {
      kContextVariable = 0, // v8::HeapGraphEdge::kContextVariable,
      kElement = 1, // v8::HeapGraphEdge::kElement,
      kProperty = 2, // v8::HeapGraphEdge::kProperty,
      kInternal = 3, // v8::HeapGraphEdge::kInternal,
      kHidden = 4, // v8::HeapGraphEdge::kHidden,
      kShortcut = 5, // v8::HeapGraphEdge::kShortcut,
      kWeak = 6 // v8::HeapGraphEdge::kWeak
    };

    Type type;
    uint64_t index_or_property;
    uint64_t to_id;
    uint64_t to_addr;
  };

  ~V8SnapshotCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  void PrepareData(v8::Error &err);

  Node::Type GetInstanceType(v8::Error &err, uint64_t word);
  uint64_t GetStringId(v8::Error &err, std::string name);
  uint64_t GetSelfSize(v8::Error &err, uint64_t word);
  Edge CreateEdgeFromValue(v8::Error &err, v8::HeapObject &obj, v8::Value value);
  uint64_t ChildrenCount(v8::Error &err, uint64_t word);

  bool SerializeImpl(v8::Error &err);

  void SerializeSnapshot(v8::Error &err);

  void SerializeNodes(v8::Error &err);
  void SerializeNode(v8::Error &err, Node* node, bool first_node);

  void SerializeEdges(v8::Error &err);
  void SerializeEdge(v8::Error &err, Edge edge, bool first_edge);

  // TODO
  void SerializeTraceNodeInfos(v8::Error &err);

  // TODO
  void SerializeTraceTree(v8::Error &err);
  void SerializeTraceNode(v8::Error &err);

  // TODO
  void SerializeSamples(v8::Error &err);

  // TODO
  void SerializeStrings(v8::Error &err);
  void SerializeString(v8::Error &err, std::string s);

 private:
  std::ofstream writer_;
  std::vector<Node> nodes_;
  std::vector<Edge> edges_;
  std::vector<std::string> strings_;
};

}  // namespace llnode

#endif  // SRC_LLNODE_H_
