#include <assert.h>
#include <string.h>

#include "llv8.h"
#include "llv8-inl.h"

namespace llnode {

using namespace lldb;

static std::string kConstantPrefix = "v8dbg_";

LLV8::LLV8(SBTarget target) : target_(target), process_(target_.GetProcess()) {
  smi_.kTag = LoadConstant("SmiTag");
  smi_.kTagMask = LoadConstant("SmiTagMask");
  smi_.kShiftSize = LoadConstant("SmiShiftSize");

  heap_obj_.kTag = LoadConstant("HeapObjectTag");
  heap_obj_.kTagMask = LoadConstant("HeapObjectTagMask");
  heap_obj_.kMapOffset = LoadConstant("class_HeapObject__map__Map");

  map_.kInstanceAttrsOffset =
      LoadConstant("class_Map__instance_attributes__int");

  js_function_.kSharedInfoOffset =
      LoadConstant("class_JSFunction__shared__SharedFunctionInfo");

  shared_info_.kNameOffset =
      LoadConstant("class_SharedFunctionInfo__name__Object");
  shared_info_.kInferredNameOffset =
      LoadConstant("class_SharedFunctionInfo__inferred_name__String");
  shared_info_.kScriptOffset =
      LoadConstant("class_SharedFunctionInfo__script__Object");

  script_.kNameOffset = LoadConstant("class_Script__name__Object");
  script_.kLineOffset = LoadConstant("class_Script__line_offset__SMI");

  string_.kEncodingMask = LoadConstant("StringEncodingMask");
  string_.kRepresentationMask = LoadConstant("StringRepresentationMask");

  string_.kOneByteStringTag = LoadConstant("OneByteStringTag");
  string_.kTwoByteStringTag = LoadConstant("TwoByteStringTag");
  string_.kSeqStringTag = LoadConstant("SeqStringTag");
  string_.kConsStringTag = LoadConstant("ConsStringTag");

  string_.kLengthOffset = LoadConstant("class_String__length__SMI");

  one_byte_string_.kCharsOffset =
      LoadConstant("class_SeqOneByteString__chars__char");

  two_byte_string_.kCharsOffset =
      LoadConstant("class_SeqTwoByteString__chars__char");

  cons_string_.kFirstOffset = LoadConstant("class_ConsString__first__String");
  cons_string_.kSecondOffset = LoadConstant("class_ConsString__second__String");

  frame_.kContextOffset = LoadConstant("off_fp_context");
  frame_.kFunctionOffset = LoadConstant("off_fp_function");
  frame_.kArgsOffset = LoadConstant("off_fp_args");
  frame_.kMarkerOffset = LoadConstant("off_fp_marker");

  frame_.kAdaptorFrame = LoadConstant("frametype_ArgumentsAdaptorFrame");
  frame_.kEntryFrame = LoadConstant("frametype_EntryFrame");
  frame_.kEntryConstructFrame = LoadConstant("frametype_EntryConstructFrame");
  frame_.kExitFrame = LoadConstant("frametype_ExitFrame");
  frame_.kInternalFrame = LoadConstant("frametype_InternalFrame");
  frame_.kConstructFrame = LoadConstant("frametype_ConstructFrame");
  frame_.kJSFrame = LoadConstant("frametype_JavaScriptFrame");
  frame_.kOptimizedFrame = LoadConstant("frametype_OptimizedFrame");

  types_.kFirstNonstringType = LoadConstant("FirstNonstringType");

  types_.kCodeType = LoadConstant("type_Code__CODE_TYPE");
  types_.kJSFunctionType = LoadConstant("type_JSFunction__JS_FUNCTION_TYPE");
}


int64_t LLV8::LoadConstant(const char* name) {
  return target_.FindFirstGlobalVariable((kConstantPrefix + name).c_str())
                .GetValueAsSigned(-1);
}


bool LLV8::LoadPtr(int64_t addr, int64_t* out) {
  SBError err;
  int64_t value = process_.ReadPointerFromMemory(static_cast<addr_t>(addr),
      err);
  if (err.Fail())
    return false;

  *out = value;
  return true;
}


bool LLV8::LoadString(int64_t addr, int64_t length, std::string& out) {
  char* buf = new char[length + 1];
  SBError err;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length), err);
  if (err.Fail())
    return false;

  buf[length] = '\0';

  out = buf;
  delete[] buf;
  return true;
}


bool LLV8::LoadTwoByteString(int64_t addr, int64_t length, std::string& out) {
  char* buf = new char[length * 2 + 1];
  SBError err;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length * 2), err);
  if (err.Fail())
    return false;

  for (int64_t i = 0; i < length; i++)
    buf[i] = buf[i * 2];
  buf[length] = '\0';

  out = buf;
  delete[] buf;
  return true;
}


bool LLV8::GetType(int64_t addr, int64_t* out) {
  if (!is_heap_obj(addr))
    return false;

  int64_t map;
  if (!GetMap(addr, &map) || !is_heap_obj(map))
    return false;

  if (!LoadHeapField(map, map_.kInstanceAttrsOffset, out))
    return false;

  (*out) &= 0xff;
  return true;
}


bool LLV8::GetJSFrameName(addr_t frame, std::string& res) {
  int64_t context;
  if (!LoadPtr(frame, frame_.kContextOffset, &context)) return false;

  if (is_smi(context) && untag_smi(context) == frame_.kAdaptorFrame) {
    res = "<adaptor>";
    return true;
  }

  int64_t marker;
  if (!LoadPtr(frame, frame_.kMarkerOffset, &marker)) return false;

  if (is_smi(marker)) {
    marker = untag_smi(marker);
    if (marker == frame_.kEntryFrame) {
      res = "<entry>";
    } else if (marker == frame_.kEntryConstructFrame) {
      res = "<entry_construct>";
    } else if (marker == frame_.kExitFrame) {
      res = "<exit>";
    } else if (marker == frame_.kInternalFrame) {
      res = "<internal>";
    } else if (marker == frame_.kConstructFrame) {
      res = "<constructor>";
    } else if (marker != frame_.kJSFrame && marker != frame_.kOptimizedFrame) {
      return false;
    }

    if (marker != frame_.kJSFrame && marker != frame_.kOptimizedFrame)
      return true;
  }

  // We are dealing with function or internal code (probably stub)
  int64_t fn;
  int64_t args;
  if (!LoadPtr(frame, frame_.kFunctionOffset, &fn) ||
      !LoadPtr(frame, frame_.kArgsOffset, &args)) {
    return false;
  }

  // Invalid fn or args
  if (!is_heap_obj(fn) || !is_heap_obj(args)) return false;

  int64_t fn_type;
  if (!GetType(fn, &fn_type)) return false;
  if (fn_type == types_.kCodeType) {
    res = "<internal code>";
    return true;
  }

  if (fn_type != types_.kJSFunctionType) {
    res = "<non-function>";
    return true;
  }

  return GetJSFunctionName(static_cast<addr_t>(fn), res);
}


bool LLV8::GetJSFunctionName(addr_t fn, std::string& res) {
  int64_t shared_info;
  int64_t name;
  if (!LoadHeapField(static_cast<int64_t>(fn), js_function_.kSharedInfoOffset,
                     &shared_info)) {
    return false;
  }

  if (!LoadHeapField(shared_info, shared_info_.kNameOffset, &name))
    return false;

  if (!ToString(static_cast<addr_t>(name), res)) {
    if (LoadHeapField(shared_info, shared_info_.kInferredNameOffset, &name))
      return false;
    if (!ToString(static_cast<addr_t>(name), res)) return false;
  }

  if (res.empty())
    res = "(anonymous js function)";

  res += " at ";

  std::string shared;
  if (!GetSharedInfoPostfix(static_cast<int64_t>(shared_info), shared))
    return false;

  res += shared;

  return true;
}


bool LLV8::GetSharedInfoPostfix(lldb::addr_t addr, std::string& res) {
  int64_t info = static_cast<int64_t>(addr);

  int64_t script;
  if (!LoadHeapField(info, shared_info_.kScriptOffset, &script)) return false;
  if (!is_heap_obj(script)) return false;

  int64_t name;
  int64_t line;
  if (!LoadHeapField(script, script_.kNameOffset, &name) ||
      !LoadHeapField(script, script_.kLineOffset, &line)) {
    return false;
  }

  if (!is_heap_obj(name) || !is_smi(line)) return false;

  if (!ToString(name, res)) return false;

  if (res.empty())
    res = "(no script)";

  char buf[1024];
  snprintf(buf, sizeof(buf), ":%d", static_cast<int>(untag_smi(line)));
  res += buf;

  return true;
}


bool LLV8::ToString(addr_t value, std::string& res) {
  int64_t obj = static_cast<int64_t>(value);
  if (is_smi(obj)) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%d", static_cast<int>(untag_smi(obj)));
    res = buf;
    return true;
  }

  int64_t type;
  if (!GetType(obj, &type)) return false;

  if (type == types_.kJSFunctionType) {
    res = "<function: ";

    std::string tmp;
    if (!GetJSFunctionName(value, tmp)) return false;

    res += tmp;
    res += ">";
    return true;
  }

  if (type > types_.kFirstNonstringType) {
    res = "<unknown non-string>";
    return true;
  }

  int64_t encoding = type & string_.kEncodingMask;
  int64_t repr = type & string_.kRepresentationMask;

  if (repr == string_.kSeqStringTag) {
    int64_t len;
    if (!LoadHeapField(obj, string_.kLengthOffset, &len)) return false;
    if (!is_smi(len)) return false;

    if (encoding == string_.kOneByteStringTag) {
      int64_t chars = LeaHeapField(obj, one_byte_string_.kCharsOffset);
      return LoadString(chars, untag_smi(len), res);
    } else if (encoding == string_.kTwoByteStringTag) {
      int64_t chars = LeaHeapField(obj, two_byte_string_.kCharsOffset);
      return LoadTwoByteString(chars, untag_smi(len), res);
    }

    res = "<unsupported string>";
    return true;
  }

  if (repr == string_.kConsStringTag) {
    int64_t first;
    int64_t second;
    if (!LoadHeapField(obj, cons_string_.kFirstOffset, &first) ||
        !LoadHeapField(obj, cons_string_.kSecondOffset, &second)) {
      return false;
    }

    std::string tmp;
    if (!ToString(first, res) || !ToString(second, tmp)) return false;

    res += tmp;
    return true;
  }

  res = "<unsupported string>";
  return true;
}


}  // namespace llnode
