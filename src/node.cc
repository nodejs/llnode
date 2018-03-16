#include "node.h"

namespace llnode {
namespace node {

addr_t BaseObject::Persistent(Error& err) {
  lldb::SBError sberr;

  addr_t persistent_ptr = raw_ + node_->base_object()->kPersistentHandleOffset;
  addr_t persistent =
      node_->process().ReadPointerFromMemory(persistent_ptr, sberr);
  if (sberr.Fail()) {
    err = Error::Failure("Failed to load persistent handle");
    return 0;
  }
  return persistent;
}

addr_t BaseObject::Object(Error& err) {
  lldb::SBError sberr;

  addr_t persistent = Persistent(err);
  addr_t obj = node_->process().ReadPointerFromMemory(persistent, sberr);
  if (sberr.Fail()) {
    err = Error::Failure("Failed to load object from persistent handle");
    return 0;
  }
  return obj;
}

HandleWrap HandleWrap::GetItemFromList(Node* node, addr_t list_node_addr) {
  return HandleWrap(node,
                    list_node_addr - node->handle_wrap()->kListNodeOffset);
}

ReqWrap ReqWrap::GetItemFromList(Node* node, addr_t list_node_addr) {
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
