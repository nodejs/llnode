#ifndef SRC_CONSTANTS_H_
#define SRC_CONSTANTS_H_

#include <lldb/API/LLDB.h>
#include <string>

#include "src/error.h"

using lldb::SBTarget;

namespace llnode {

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

  static int64_t LookupConstant(SBTarget target, const char* name, int64_t def,
                                Error& err);

 protected:
  int64_t LoadRawConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, Error& err, int64_t def = -1);
  int64_t LoadConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, const char* fallback,
                       int64_t def = -1);

  lldb::SBTarget target_;
  bool loaded_;
};

}  // namespace llnode

#endif
