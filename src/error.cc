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


void Error::PrintInDebugMode(const char* file, int line, const char* funcname,
                             const char* format, ...) {
  if (!is_debug_mode) {
    return;
  }
  char fmt[kMaxMessageLength];
  snprintf(fmt, sizeof(fmt), "[llnode][%s %s:%d] %s\n", funcname, file, line,
           format);
  va_list arglist;
  va_start(arglist, format);
  vfprintf(stderr, fmt, arglist);
  va_end(arglist);
}


Error Error::Failure(std::string msg) {
  // TODO(mmarchini): The file and function information here won't be relevant.
  // But then again, maybe we should rethink Error::Failure.
  PRINT_DEBUG("%s", msg.c_str());
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
