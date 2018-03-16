#ifndef SRC_NODE_H_
#define SRC_NODE_H_

#include <lldb/API/LLDB.h>
#include <list>

#include "node-constants.h"

#define CONSTANTS_LIST(V)               \
  V(Environment, env)                   \
  V(ReqWrapQueue, req_wrap_queue)       \
  V(ReqWrap, req_wrap)                  \
  V(HandleWrapQueue, handle_wrap_queue) \
  V(HandleWrap, handle_wrap)            \
  V(BaseObject, base_object)

namespace llnode {
namespace node {

class Node;
class HandleWrap;
class ReqWrap;
template <typename T, typename C>
class Queue;

class BaseNode {
 public:
  BaseNode(Node* node) : node_(node){};

 protected:
  Node* node_;
};

typedef Queue<HandleWrap, constants::HandleWrapQueue> HandleWrapQueue;
typedef Queue<ReqWrap, constants::ReqWrapQueue> ReqWrapQueue;

class Environment : public BaseNode {
 public:
  Environment(Node* node, addr_t raw) : BaseNode(node), raw_(raw){};
  inline addr_t raw() { return raw_; };

  static Environment GetCurrent(Node* node, Error& err);

  HandleWrapQueue handle_wrap_queue() const;
  ReqWrapQueue req_wrap_queue() const;

 private:
  addr_t raw_;
};

class BaseObject : public BaseNode {
 public:
  BaseObject(Node* node, addr_t raw) : BaseNode(node), raw_(raw){};
  inline addr_t raw() { return raw_; };

  addr_t persistent_addr(Error& err);

  addr_t v8_object_addr(Error& err);


 private:
  addr_t raw_;
};

class AsyncWrap : public BaseObject {
 public:
  AsyncWrap(Node* node, addr_t raw) : BaseObject(node, raw){};
};

class HandleWrap : public AsyncWrap {
 public:
  HandleWrap(Node* node, addr_t raw) : AsyncWrap(node, raw){};

  static HandleWrap FromListNode(Node* node, addr_t list_node_addr);
};

class ReqWrap : public AsyncWrap {
 public:
  ReqWrap(Node* node, addr_t raw) : AsyncWrap(node, raw){};

  static ReqWrap FromListNode(Node* node, addr_t list_node_addr);
};

class Node {
 public:
#define V(Class, Attribute) Attribute(constants::Class(llv8)),
  Node(v8::LLV8* llv8) : CONSTANTS_LIST(V) target_(lldb::SBTarget()) {}
#undef V

  inline lldb::SBProcess process() { return process_; };

  void Load(lldb::SBTarget target);

#define V(Class, Attribute) constants::Class Attribute;
  CONSTANTS_LIST(V)
#undef V

 private:
  lldb::SBTarget target_;
  lldb::SBProcess process_;
};

template <typename T, typename C>
class Queue : public BaseNode {
  class Iterator : public BaseNode {
   public:
    inline T operator*() const;
    inline const Iterator operator++();
    inline bool operator!=(const Iterator& that) const;


    inline Iterator(Node* node, addr_t current, C* constants)
        : BaseNode(node), current_(current), constants_(constants){};

   public:
    addr_t current_;
    C* constants_;
  };

 public:
  inline Queue(Node* node, addr_t raw, C* constants)
      : BaseNode(node), raw_(raw), constants_(constants){};

  inline Iterator begin() const;
  inline Iterator end() const;

 private:
  addr_t raw_;
  C* constants_;
};
}  // namespace node
}  // namespace llnode

#endif
