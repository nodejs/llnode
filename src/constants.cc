#include <cinttypes>

#include <lldb/API/SBExpressionOptions.h>

#include "src/constants.h"

using lldb::SBAddress;
using lldb::SBError;
using lldb::SBSymbol;
using lldb::SBSymbolContext;
using lldb::SBSymbolContextList;

namespace llnode {

template <typename T>
T ReadSymbolFromTarget(SBTarget& target, SBAddress& start, const char* name,
                       Error& err) {
  SBError sberr;
  T res = 0;
  target.ReadMemory(start, &res, sizeof(T), sberr);
  if (!sberr.Fail()) {
    err = Error::Ok();
  } else {
    err = Error::Failure("Failed to read symbol %s", name);
  }
  return res;
}

int64_t Constants::LookupConstant(SBTarget target, const char* name,
                                  int64_t def, Error& err) {
  int64_t res = 0;
  res = def;

  SBSymbolContextList context_list = target.FindSymbols(name);

  if (!context_list.IsValid() || context_list.GetSize() == 0) {
    err = Error::Failure("Failed to find symbol %s", name);
    return res;
  }

  SBSymbolContext context = context_list.GetContextAtIndex(0);
  SBSymbol symbol = context.GetSymbol();
  if (!symbol.IsValid()) {
    err = Error::Failure("Failed to fetch symbol %s from context", name);
    return res;
  }

  SBAddress start = symbol.GetStartAddress();
  SBAddress end = symbol.GetEndAddress();
  uint32_t size = end.GetOffset() - start.GetOffset();

  // NOTE: size could be bigger for at the end symbols
  if (size >= 8) {
    res = ReadSymbolFromTarget<int64_t>(target, start, name, err);
  } else if (size == 4) {
    int32_t tmp = ReadSymbolFromTarget<int32_t>(target, start, name, err);
    res = static_cast<int64_t>(tmp);
  } else if (size == 2) {
    int16_t tmp = ReadSymbolFromTarget<int16_t>(target, start, name, err);
    res = static_cast<int64_t>(tmp);
  } else if (size == 1) {
    int8_t tmp = ReadSymbolFromTarget<int8_t>(target, start, name, err);
    res = static_cast<int64_t>(tmp);
  } else {
    err = Error::Failure("Unexpected symbol size %" PRIu32 " of symbol %s",
                         size, name);
  }

  return res;
}

void Constants::Assign(SBTarget target) {
  loaded_ = false;
  target_ = target;
}


int64_t Constants::LoadRawConstant(const char* name, int64_t def) {
  Error err;
  int64_t v = Constants::LookupConstant(target_, name, def, err);
  if (err.Fail()) {
    Error::PrintInDebugMode(
        "Failed to load raw constant %s, default to %" PRId64, name, def);
  }

  return v;
}

int64_t Constants::LoadConstant(const char* name, Error& err, int64_t def) {
  int64_t v = Constants::LookupConstant(
      target_, (constant_prefix() + name).c_str(), def, err);
  return v;
}

int64_t Constants::LoadConstant(const char* name, int64_t def) {
  Error err;
  int64_t v = LoadConstant(name, err, def);
  if (err.Fail()) {
    Error::PrintInDebugMode("Failed to load constant %s, default to %" PRId64,
                            name, def);
  }

  return v;
}

int64_t Constants::LoadConstant(const char* name, const char* fallback,
                                int64_t def) {
  Error err;
  int64_t v = LoadConstant(name, err, def);
  if (err.Fail()) v = LoadConstant(fallback, err, def);
  if (err.Fail()) {
    Error::PrintInDebugMode(
        "Failed to load constant %s, fallback %s, default to %" PRId64, name,
        fallback, def);
  }

  return v;
}


}  // namespace llnode
