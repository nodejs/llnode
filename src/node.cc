#include "node.h"

namespace llnode {
namespace node {

addr_t BaseObject::persistent_addr(Error& err) {
  lldb::SBError sberr;

  addr_t persistentHandlePtr =
      raw_ + node_->base_object()->kPersistentHandleOffset;
  addr_t persistentHandle =
      node_->process().ReadPointerFromMemory(persistentHandlePtr, sberr);
  if (sberr.Fail()) {
    err = Error::Failure("Failed to load persistent handle");
    return 0;
  }
  return persistentHandle;
}

addr_t BaseObject::v8_object_addr(Error& err) {
  lldb::SBError sberr;

  addr_t persistentHandle = persistent_addr(err);
  addr_t obj = node_->process().ReadPointerFromMemory(persistentHandle, sberr);
  if (sberr.Fail()) {
    err = Error::Failure("Failed to load object from persistent handle");
    return 0;
  }
  return obj;
}

HandleWrap HandleWrap::FromListNode(Node* node, addr_t list_node_addr) {
  return HandleWrap(node,
                    list_node_addr - node->handle_wrap()->kListNodeOffset);
}

ReqWrap ReqWrap::FromListNode(Node* node, addr_t list_node_addr) {
  return ReqWrap(node, list_node_addr - node->req_wrap()->kListNodeOffset);
}

Environment Environment::GetCurrent(Node* node, Error& err) {
  addr_t envAddr = node->env()->kCurrentEnvironment;
  if (envAddr == 0) {
    err = Error::Failure("Couldn't get node's Environment");
  }

  return Environment(node, envAddr);
}

HandleWrapQueue Environment::handle_wrap_queue() const {
  return HandleWrapQueue(node_, raw_ + node_->env()->kHandleWrapQueueOffset,
                         node_->handle_wrap_queue());
}

ReqWrapQueue Environment::req_wrap_queue() const {
  return ReqWrapQueue(node_, raw_ + node_->env()->kReqWrapQueueOffset,
                      node_->req_wrap_queue());
}

void Node::Load(SBTarget target) {
  // Reload process anyway
  process_ = target.GetProcess();

  // No need to reload
  if (target_ == target) return;

  target_ = target;

  env.Assign(target);
  req_wrap_queue.Assign(target);
  req_wrap.Assign(target);
  handle_wrap_queue.Assign(target);
  handle_wrap.Assign(target);
  base_object.Assign(target);
}
}  // namespace node
}  // namespace llnode
