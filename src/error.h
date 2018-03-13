#ifndef SRC_ERROR_H_
#define SRC_ERROR_H_

#include <string>

namespace llnode {

class Error {
 public:
  Error() : failed_(false), msg_("") {}
  Error(bool failed, std::string msg) : failed_(failed), msg_(msg) {}
  Error(bool failed, const char* format, ...)
      __attribute__((format(printf, 3, 4)));

  static inline Error Ok() { return Error(false, "ok"); }
  static Error Failure(std::string msg);
  static Error Failure(const char* format, ...)
      __attribute__((format(printf, 1, 2)));
  static void PrintInDebugMode(const char* format, ...)
      __attribute__((format(printf, 1, 2)));

  inline bool Success() const { return !Fail(); }
  inline bool Fail() const { return failed_; }

  inline const char* GetMessage() { return msg_.c_str(); }

  static void SetDebugMode(bool mode) { is_debug_mode = mode; }

 private:
  bool failed_;
  std::string msg_;
  static const size_t kMaxMessageLength = 128;
  static bool is_debug_mode;
};
}  // namespace llnode

#endif
