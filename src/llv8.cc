#include <assert.h>
#include <string.h>

#include "llv8.h"
#include "llv8-inl.h"

namespace llnode {
namespace v8 {

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
  shared_info_.kStartPositionOffset =
      LoadConstant("class_SharedFunctionInfo__start_position_and_type__SMI");

  // TODO(indutny): move it to post-mortem
  shared_info_.kStartPositionShift = 2;

  script_.kNameOffset = LoadConstant("class_Script__name__Object");

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


int64_t LLV8::LoadPtr(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadPointerFromMemory(static_cast<addr_t>(addr),
      sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 value");
    return -1;
  }

  err = Error::Ok();
  return value;
}


std::string LLV8::LoadString(int64_t addr, int64_t length, Error& err) {
  char* buf = new char[length + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 one byte string");
    return std::string();
  }

  buf[length] = '\0';

  std::string res = buf;
  delete[] buf;
  err = Error::Ok();
  return res;
}


std::string LLV8::LoadTwoByteString(int64_t addr, int64_t length, Error& err) {
  char* buf = new char[length * 2 + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length * 2), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 one byte string");
    return std::string();
  }

  for (int64_t i = 0; i < length; i++)
    buf[i] = buf[i * 2];
  buf[length] = '\0';

  std::string res = buf;
  delete[] buf;
  err = Error::Ok();
  return res;
}


std::string LLV8::GetJSFrameName(Value frame, Error& err) {
  Value context = LoadValue<Value>(frame.raw() + frame_.kContextOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_context = Smi(this, context.raw());
  if (smi_context.Check() && smi_context.GetValue() == frame_.kAdaptorFrame)
    return "<adaptor>";

  Value marker = LoadValue<Value>(frame.raw() + frame_.kMarkerOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_marker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == frame_.kEntryFrame) {
      return "<entry>";
    } else if (value == frame_.kEntryConstructFrame) {
      return "<entry_construct>";
    } else if (value == frame_.kExitFrame) {
      return "<exit>";
    } else if (value == frame_.kInternalFrame) {
      return "<internal>";
    } else if (value == frame_.kConstructFrame) {
      return "<constructor>";
    } else if (value != frame_.kJSFrame && value != frame_.kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker");
      return std::string();
    }
  }

  // We are dealing with function or internal code (probably stub)
  JSFunction fn =
      LoadValue<JSFunction>(frame.raw() + frame_.kFunctionOffset, err);
  if (err.Fail()) return std::string();

  LoadValue<HeapObject>(frame.raw() + frame_.kArgsOffset, err);
  if (err.Fail()) return std::string();

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) return std::string();

  if (fn_type == types_.kCodeType) return "<internal code>";
  if (fn_type != types_.kJSFunctionType) return "<non-function>";

  return fn.GetDebugLine(err);
}


std::string JSFunction::GetDebugLine(Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return std::string();

  String name = info.Name(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (err.Fail()) {
    name = info.InferredName(err);
    if (err.Fail()) return std::string();

    res = name.ToString(err);
    if (err.Fail()) return std::string();
  }

  if (res.empty())
    res = "(anonymous js function)";

  res += " at ";

  std::string shared;

  res += info.GetPostfix(err);
  if (err.Fail()) return std::string();

  return res;
}


std::string SharedFunctionInfo::GetPostfix(Error& err) {
  Script script = GetScript(err);
  if (err.Fail()) return std::string();

  String name = script.Name(err);
  if (err.Fail()) return std::string();

  int64_t start_pos = StartPosition(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (res.empty())
    res = "(no script)";

  char buf[1024];
  snprintf(buf, sizeof(buf), ":%d", static_cast<int>(start_pos));
  res += buf;

  return res;
}


std::string Value::ToString(Error& err) {
  Smi smi(this);
  if (smi.Check())
    return smi.ToString(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  int64_t type = obj.GetType(err);
  if (err.Fail()) return std::string();

  if (type == v8()->types_.kJSFunctionType) {
    JSFunction fn(this);
    return "<function: " + fn.GetDebugLine(err) + ">";
  }

  if (type > v8()->types_.kFirstNonstringType) return "<unknown non-string>";

  String str(this);
  return str.ToString(err);
}


std::string Smi::ToString(Error& err) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", static_cast<int>(GetValue()));
  err = Error::Ok();
  return buf;
}


std::string String::ToString(Error& err) {
  int64_t repr = Representation(err);
  if (err.Fail()) return std::string();

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (repr == v8()->string_.kSeqStringTag) {
    if (encoding == v8()->string_.kOneByteStringTag) {
      OneByteString one(this);
      return one.GetValue(err);
    } else if (encoding == v8()->string_.kTwoByteStringTag) {
      TwoByteString two(this);
      return two.GetValue(err);
    }

    return "<unsupported string>";
  }

  if (repr == v8()->string_.kConsStringTag) {
    ConsString cons(this);
    String first = cons.First(err);
    if (err.Fail()) return std::string();

    String second = cons.Second(err);
    if (err.Fail()) return std::string();

    std::string tmp = first.ToString(err);
    if (err.Fail()) return std::string();
    tmp += second.ToString(err);
    if (err.Fail()) return std::string();

    return tmp;
  }

  return "<unsupported string>";
}

}  // namespace v8
}  // namespace llnode
