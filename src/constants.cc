#include <cinttypes>
#include <initializer_list>
#include <iterator>
#include <sstream>

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
                       SBError& sberr) {
  T res = 0;
  target.ReadMemory(start, &res, sizeof(T), sberr);
  return res;
}

std::pair<int64_t, bool> Constants::LookupConstant(SBTarget target,
                                                   const char* name,
                                                   int64_t def) {
  int64_t res = 0;
  res = def;

  SBSymbolContextList context_list = target.FindSymbols(name);

  if (!context_list.IsValid() || context_list.GetSize() == 0) {
    return {res, false};
  }

  SBSymbolContext context = context_list.GetContextAtIndex(0);
  SBSymbol symbol = context.GetSymbol();
  if (!symbol.IsValid()) {
    return {res, false};
  }

  SBAddress start = symbol.GetStartAddress();
  SBAddress end = symbol.GetEndAddress();
  uint32_t size = end.GetOffset() - start.GetOffset();

  // NOTE: size could be bigger for at the end symbols
  SBError sberr;
  if (size >= 8) {
    res = ReadSymbolFromTarget<int64_t>(target, start, name, sberr);
  } else if (size == 4) {
    int32_t tmp = ReadSymbolFromTarget<int32_t>(target, start, name, sberr);
    res = static_cast<int64_t>(tmp);
  } else if (size == 2) {
    int16_t tmp = ReadSymbolFromTarget<int16_t>(target, start, name, sberr);
    res = static_cast<int64_t>(tmp);
  } else if (size == 1) {
    int8_t tmp = ReadSymbolFromTarget<int8_t>(target, start, name, sberr);
    res = static_cast<int64_t>(tmp);
  } else {
    return {res, false};
  }

  return {res, !sberr.Fail()};
}

void Constants::Assign(SBTarget target) {
  loaded_ = false;
  target_ = target;
}


int64_t Constants::LoadRawConstant(const char* name, int64_t def) {
  auto v = Constants::LookupConstant(target_, name, def);
  if (!v.second) {
    PRINT_DEBUG("Failed to load raw constant %s, default to %" PRId64, name,
                def);
  }

  return v.first;
}

int64_t Constants::LoadConstant(const char* name, int64_t def) {
  auto v = Constants::LookupConstant(target_,
                                     (constant_prefix() + name).c_str(), def);
  if (!v.second) {
    PRINT_DEBUG("Failed to load constant %s, default to %" PRId64, name, def);
  }

  return v.first;
}

int64_t Constants::LoadConstant(const char* name, const char* fallback,
                                int64_t def) {
  auto v = Constants::LookupConstant(target_,
                                     (constant_prefix() + name).c_str(), def);
  if (!v.second)
    v = Constants::LookupConstant(target_,
                                  (constant_prefix() + fallback).c_str(), def);
  if (!v.second) {
    PRINT_DEBUG("Failed to load constant %s, fallback %s, default to %" PRId64,
                name, fallback, def);
  }

  return v.first;
}

Constant<int64_t> Constants::LoadConstant(
    std::initializer_list<const char*> names) {
  for (std::string name : names) {
    auto v = Constants::LookupConstant(target_,
                                       (constant_prefix() + name).c_str(), -1);
    if (v.second) return Constant<int64_t>(v.first, name);
  }

  if (Error::IsDebugMode()) {
    std::string joined = "";
    for (std::string name : names) {
      joined += (joined.empty() ? "'" : ", '") + name + "'";
    }
    PRINT_DEBUG("Failed to load constants: %s", joined.c_str());
  }

  return Constant<int64_t>();
}

Constant<int64_t> Constants::LoadOptionalConstant(
    std::initializer_list<const char*> names, int def) {
  for (std::string name : names) {
    auto v = Constants::LookupConstant(target_,
                                       (constant_prefix() + name).c_str(), -1);
    if (v.second) return Constant<int64_t>(v.first, name);
  }

  return Constant<int64_t>(def);
}

}  // namespace llnode
