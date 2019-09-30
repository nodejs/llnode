#ifndef SRC_CONSTANTS_H_
#define SRC_CONSTANTS_H_

#include <lldb/API/LLDB.h>
#include <string>

#include "src/error.h"

using lldb::SBTarget;

namespace llnode {

template <typename T>
class Constant {
 public:
  Constant() : value_(-1), valid_(false), loaded_(false) {}
  explicit Constant(T value)
      : value_(value), valid_(true), loaded_(false), name_("") {}
  Constant(T value, std::string name)
      : value_(value), valid_(true), loaded_(true), name_(name) {}

  inline bool Check() { return valid_; }

  inline bool Loaded() { return loaded_; }

  T operator*() {
    // TODO(mmarchini): Check()
    return value_;
  }

  inline std::string name() { return name_; }

 protected:
  T value_;
  bool valid_;
  bool loaded_;
  std::string name_;
};

#define CONSTANTS_DEFAULT_METHODS(NAME) \
  inline NAME* operator()() {           \
    if (loaded_) return this;           \
    loaded_ = true;                     \
    Load();                             \
    return this;                        \
  }

class Constants {
 public:
  Constants() : loaded_(false) {}

  inline bool is_loaded() const { return loaded_; }

  void Assign(lldb::SBTarget target);

  inline virtual std::string constant_prefix() { return ""; };

  static std::pair<int64_t, bool> LookupConstant(SBTarget target,
                                                 const char* name, int64_t def);

 protected:
  int64_t LoadRawConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, const char* fallback,
                       int64_t def = -1);
  Constant<int64_t> LoadConstant(std::initializer_list<const char*> names);
  Constant<int64_t> LoadOptionalConstant(
      std::initializer_list<const char*> names, int def);

  lldb::SBTarget target_;
  bool loaded_;
};

}  // namespace llnode

#endif
