#include "node.h"

namespace llnode {
namespace node {

template <typename T, typename C>
T Queue<T, C>::Iterator::operator*() const {
  return T::GetItemFromList(node_, current_);
}

template <typename T, typename C>
const typename Queue<T, C>::Iterator Queue<T, C>::Iterator::operator++() {
  lldb::SBError sberr;

  current_ = node_->process().ReadPointerFromMemory(
      current_ + constants_->kNextOffset, sberr);
  return Iterator(node_, current_, constants_);
}

template <typename T, typename C>
bool Queue<T, C>::Iterator::operator!=(const Iterator& that) const {
  return current_ != that.current_;
}

template <typename T, typename C>
typename Queue<T, C>::Iterator Queue<T, C>::begin() const {
  lldb::SBError sberr;

  addr_t first = node_->process().ReadPointerFromMemory(next(head()), sberr);
  return Iterator(node_, first, constants_);
}

template <typename T, typename C>
typename Queue<T, C>::Iterator Queue<T, C>::end() const {
  return Iterator(node_, head(), constants_);
}
}  // namespace node
}  // namespace llnode
