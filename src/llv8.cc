#include <assert.h>
#include <string.h>

#include "llv8.h"
#include "llv8-inl.h"

namespace llnode {

using namespace lldb;

static std::string kConstantPrefix = std::string("v8dbg_");

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


int64_t LLV8::LoadPtr(int64_t addr) {
  SBError err;
  int64_t value = process_.ReadPointerFromMemory(static_cast<addr_t>(addr),
      err);
  assert(!err.Fail());
  return value;
}


std::string LLV8::LoadString(int64_t addr, int64_t length) {
  char* buf = new char[length + 1];
  SBError err;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length), err);
  assert(!err.Fail());
  buf[length] = '\0';

  std::string res = std::string(buf);
  delete[] buf;
  return res;
}


std::string LLV8::LoadTwoByteString(int64_t addr, int64_t length) {
  char* buf = new char[length * 2 + 1];
  SBError err;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length * 2), err);
  assert(!err.Fail());

  for (int64_t i = 0; i < length; i++)
    buf[i] = buf[i * 2];
  buf[length] = '\0';

  std::string res = std::string(buf);
  delete[] buf;
  return res;
}


int64_t LLV8::GetType(int64_t addr) {
  if (!is_heap_obj(addr))
    return -1;

  int64_t map = GetMap(addr);
  if (!is_heap_obj(map))
    return -1;

  return LoadHeapField(map, map_.kInstanceAttrsOffset) & 0xff;
}


std::string LLV8::GetJSFrameName(addr_t frame) {
  int64_t context = LoadPtr(frame, frame_.kContextOffset);
  if (is_smi(context) && untag_smi(context) == frame_.kAdaptorFrame)
    return std::string("<adaptor>");

  int64_t marker = LoadPtr(frame, frame_.kMarkerOffset);
  if (is_smi(marker)) {
    marker = untag_smi(marker);
    if (marker == frame_.kEntryFrame) return std::string("<entry>");
    if (marker == frame_.kEntryConstructFrame)
      return std::string("<entry_construct>");
    if (marker == frame_.kExitFrame) return std::string("<exit>");
    if (marker == frame_.kInternalFrame) return std::string("<internal>");
    if (marker == frame_.kConstructFrame) return std::string("<constructor>");
    if (marker != frame_.kJSFrame && marker != frame_.kOptimizedFrame)
      return std::string("<invalid marker>");
  }

  // We are dealing with function or internal code (probably stub)
  int64_t fn = LoadPtr(frame, frame_.kFunctionOffset);
  int64_t args = LoadPtr(frame, frame_.kArgsOffset);
  if (!is_heap_obj(fn) || !is_heap_obj(args))
    return std::string("<invalid fn or args>");

  int64_t fn_type = GetType(fn);
  if (fn_type == types_.kCodeType) return std::string("<internal code>");

  if (fn_type != types_.kJSFunctionType)
    return std::string("<non-function>");

  return GetJSFunctionName(static_cast<addr_t>(fn));
}


std::string LLV8::GetJSFunctionName(addr_t fn) {
  int64_t shared_info = LoadHeapField(static_cast<int64_t>(fn),
      js_function_.kSharedInfoOffset);
  int64_t name = LoadHeapField(shared_info, shared_info_.kNameOffset);

  std::string res = ToString(static_cast<addr_t>(name));
  if (res.empty()) {
    name = LoadHeapField(shared_info, shared_info_.kInferredNameOffset);
    res = ToString(static_cast<addr_t>(name));
  }

  if (res.empty())
    res = std::string("(anonymous js function)");

  res += " at ";
  res += GetSharedInfoPostfix(static_cast<int64_t>(shared_info));

  return res;
}


std::string LLV8::GetSharedInfoPostfix(lldb::addr_t addr) {
  int64_t info = static_cast<int64_t>(addr);
  int64_t script = LoadHeapField(info, shared_info_.kScriptOffset);
  if (!is_heap_obj(script)) return std::string();

  int64_t name = LoadHeapField(script, script_.kNameOffset);
  int64_t line = LoadHeapField(script, script_.kLineOffset);

  if (!is_heap_obj(name) || !is_smi(line)) return std::string();

  std::string res = ToString(name);
  if (res.empty())
    res = std::string("(no script)");

  char buf[1024];
  snprintf(buf, sizeof(buf), ":%d", static_cast<int>(untag_smi(line)));
  res += buf;

  return res;
}


std::string LLV8::ToString(addr_t value) {
  int64_t obj = static_cast<int64_t>(value);
  int64_t type = GetType(obj);

  if (type > types_.kFirstNonstringType) return std::string();

  int64_t encoding = type & string_.kEncodingMask;
  int64_t repr = type & string_.kRepresentationMask;

  if (repr == string_.kSeqStringTag) {
    int64_t len = LoadHeapField(obj, string_.kLengthOffset);
    if (!is_smi(len)) return std::string();

    if (encoding == string_.kOneByteStringTag) {
      int64_t chars = LeaHeapField(obj, one_byte_string_.kCharsOffset);
      return LoadString(chars, untag_smi(len));
    } else if (encoding == string_.kTwoByteStringTag) {
      int64_t chars = LeaHeapField(obj, two_byte_string_.kCharsOffset);
      return LoadTwoByteString(chars, untag_smi(len));
    }

    return std::string();
  }

  if (repr == string_.kConsStringTag) {
    int64_t first = LoadHeapField(obj, cons_string_.kFirstOffset);
    int64_t second = LoadHeapField(obj, cons_string_.kSecondOffset);

    return ToString(first) + ToString(second);
  }

  return std::string();
}


}  // namespace llnode
