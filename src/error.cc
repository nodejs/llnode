#include <cstdarg>

#include "error.h"

namespace llnode {
bool Error::is_debug_mode = false;

Error::Error(bool failed, const char* format, ...) {
  failed_ = failed;
  char tmp[kMaxMessageLength];
  va_list arglist;
  va_start(arglist, format);
  vsnprintf(tmp, sizeof(tmp), format, arglist);
  va_end(arglist);
  msg_ = tmp;
}


void Error::PrintInDebugMode(const char* format, ...) {
  if (!is_debug_mode) {
    return;
  }
  char fmt[kMaxMessageLength];
  snprintf(fmt, sizeof(fmt), "[llv8] %s\n", format);
  va_list arglist;
  va_start(arglist, format);
  vfprintf(stderr, fmt, arglist);
  va_end(arglist);
}


Error Error::Failure(std::string msg) {
  PrintInDebugMode("%s", msg.c_str());
  return Error(true, msg);
}


Error Error::Failure(const char* format, ...) {
  char tmp[kMaxMessageLength];
  va_list arglist;
  va_start(arglist, format);
  vsnprintf(tmp, sizeof(tmp), format, arglist);
  va_end(arglist);
  return Error::Failure(std::string(tmp));
}
}  // namespace llnode
