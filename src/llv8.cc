#include <assert.h>

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <iomanip>
#include <sstream>
#include <string>

#include "deps/rang/include/rang.hpp"
#include "llv8-inl.h"
#include "llv8.h"
#include "src/settings.h"

namespace llnode {
namespace v8 {

using lldb::addr_t;
using lldb::SBError;
using lldb::SBTarget;

static std::string kConstantPrefix = "v8dbg_";

void LLV8::Load(SBTarget target) {
  // Reload process anyway
  process_ = target.GetProcess();

  // No need to reload
  if (target_ == target) return;

  target_ = target;

  common.Assign(target);
  smi.Assign(target, &common);
  heap_obj.Assign(target, &common);
  map.Assign(target, &common);
  js_object.Assign(target, &common);
  heap_number.Assign(target, &common);
  js_array.Assign(target, &common);
  js_function.Assign(target, &common);
  shared_info.Assign(target, &common);
  uncompiled_data.Assign(target, &common);
  code.Assign(target, &common);
  scope_info.Assign(target, &common);
  context.Assign(target, &common);
  script.Assign(target, &common);
  string.Assign(target, &common);
  one_byte_string.Assign(target, &common);
  two_byte_string.Assign(target, &common);
  cons_string.Assign(target, &common);
  sliced_string.Assign(target, &common);
  thin_string.Assign(target, &common);
  fixed_array_base.Assign(target, &common);
  fixed_array.Assign(target, &common);
  fixed_typed_array_base.Assign(target, &common);
  js_typed_array.Assign(target, &common);
  oddball.Assign(target, &common);
  js_array_buffer.Assign(target, &common);
  js_array_buffer_view.Assign(target, &common);
  js_regexp.Assign(target, &common);
  js_date.Assign(target, &common);
  descriptor_array.Assign(target, &common);
  name_dictionary.Assign(target, &common);
  frame.Assign(target, &common);
  symbol.Assign(target, &common);
  types.Assign(target, &common);
}

int64_t LLV8::LoadPtr(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value =
      process_.ReadPointerFromMemory(static_cast<addr_t>(addr), sberr);
  if (sberr.Fail()) {
    // TODO(joyeecheung): use Error::Failure() to report information when
    // there is less noise from here.
    err = Error(true, "Failed to load pointer from v8 memory");
    return -1;
  }

  err = Error::Ok();
  return value;
}

int64_t LLV8::LoadUnsigned(int64_t addr, uint32_t byte_size, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
                                                  byte_size, sberr);

  if (sberr.Fail()) {
    // TODO(joyeecheung): use Error::Failure() to report information when
    // there is less noise from here.
    err = Error(true, "Failed to load unsigned from v8 memory");
    return -1;
  }

  err = Error::Ok();
  return value;
}


double LLV8::LoadDouble(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
                                                  sizeof(double), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load double from v8 memory, "
        "addr=0x%016" PRIx64,
        addr);
    return -1.0;
  }

  err = Error::Ok();
  // dereferencing type-punned pointer will break strict-aliasing rules
  // return *reinterpret_cast<double*>(&value);
  double dvalue;
  std::memcpy(&dvalue, &value, sizeof(double));
  return dvalue;
}


std::string LLV8::LoadBytes(int64_t addr, size_t length, Error& err) {
  uint8_t* buf = new uint8_t[length + 1];
  SBError sberr;
  process_.ReadMemory(addr, buf, length, sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load v8 backing store memory, "
        "addr=0x%016" PRIx64 ", length=%zu",
        addr, length);
    delete[] buf;
    return std::string();
  }

  std::string res;
  char tmp[10];
  for (size_t i = 0; i < length; ++i) {
    snprintf(tmp, sizeof(tmp), "%s%02x", (i == 0 ? "" : ", "), buf[i]);
    res += tmp;
  }
  delete[] buf;
  return res;
}

std::string LLV8::LoadString(int64_t addr, int64_t length, Error& err) {
  if (length < 0) {
    err = Error::Failure("Failed to load V8 one byte string - Invalid length");
    return std::string();
  }

  char* buf = new char[length + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load v8 one byte string memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return std::string();
  }

  buf[length] = '\0';

  std::string res = buf;
  delete[] buf;
  err = Error::Ok();
  return res;
}


std::string LLV8::LoadTwoByteString(int64_t addr, int64_t length, Error& err) {
  if (length < 0) {
    err = Error::Failure("Failed to load V8 two byte string - Invalid length");
    return std::string();
  }

  char* buf = new char[length * 2 + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length * 2), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load V8 two byte string memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return std::string();
  }

  for (int64_t i = 0; i < length; i++) buf[i] = buf[i * 2];
  buf[length] = '\0';

  std::string res = buf;
  delete[] buf;
  err = Error::Ok();
  return res;
}


uint8_t* LLV8::LoadChunk(int64_t addr, int64_t length, Error& err) {
  uint8_t* buf = new uint8_t[length];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load V8 chunk memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return nullptr;
  }

  err = Error::Ok();
  return buf;
}


// reset_line - make line_start absolute vs start of function
//            otherwise relative to last end
// returns line cursor
uint32_t JSFrame::GetSourceForDisplay(bool reset_line, uint32_t line_start,
                                      uint32_t line_limit, std::string lines[],
                                      uint32_t& lines_found, Error& err) {
  v8::JSFunction fn = GetFunction(err);
  if (err.Fail()) {
    return line_start;
  }

  v8::SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) {
    return line_start;
  }

  v8::Script script = info.GetScript(err);
  if (err.Fail()) {
    return line_start;
  }

  // Check if this is being run multiple times against the same frame
  // if not, reset last line
  if (reset_line) {
    uint32_t pos = info.StartPosition(err);
    if (err.Fail()) {
      return line_start;
    }
    int64_t tmp_line;
    int64_t tmp_col;
    script.GetLineColumnFromPos(pos, tmp_line, tmp_col, err);
    if (err.Fail()) {
      return line_start;
    }
    line_start += tmp_line;
  }

  script.GetLines(line_start, lines, line_limit, lines_found, err);
  if (err.Fail()) {
    const char* msg = err.GetMessage();
    if (msg == nullptr) {
      err = Error::Failure("Failed to get Function Source");
    }
    return line_start;
  }
  return line_start + lines_found;
}


// On 64 bits systems, V8 stores SMIs (small ints) in the top 32 bits of
// a 64 bits word.  Frame markers used to obey this convention but as of
// V8 5.8, they are stored as 32 bits SMIs with the top half set to zero.
// Shift the raw value up to make it a normal SMI again.
Smi JSFrame::FromFrameMarker(Value value) const {
  if (v8()->smi()->kShiftSize == 31 && Smi(value).Check() &&
      value.raw() < 1LL << 31) {
    value = Value(v8(), value.raw() << 31);
  }
  return Smi(value);
}


bool JSFrame::MightBeV8Frame(lldb::SBFrame& frame) {
  // TODO(mmarchini): There might be a better way to check for V8 builtins
  // embedded in the binary.
  auto c_function_name = frame.GetFunctionName();
  std::string function_name(c_function_name != nullptr ? c_function_name : "");

  return !frame.GetSymbol().IsValid() || function_name.find("Builtins_") == 0;
}

std::string JSFunction::GetDebugLine(std::string args, Error& err) {
  RETURN_IF_THIS_INVALID(std::string());

  // TODO(mmarchini) turn this into CheckedType
  std::string res = Name(err);
  if (err.Fail()) return std::string();

  if (!args.empty()) res += "(" + args + ")";

  res += " at ";

  res += Info(err).GetPostfix(err);
  if (err.Fail()) return std::string();

  return res;
}


std::string JSFunction::GetSource(Error& err) {
  v8::SharedFunctionInfo info = Info(err);
  if (err.Fail()) {
    return std::string();
  }

  v8::Script script = info.GetScript(err);
  if (err.Fail()) {
    return std::string();
  }

  // There is no `Script` for functions created in C++ (and possibly others)
  int64_t type = script.GetType(err);
  if (err.Fail()) {
    return std::string();
  }

  if (type != v8()->types()->kScriptType) {
    return std::string();
  }

  HeapObject source = script.Source(err);
  if (err.Fail()) return std::string();

  int64_t source_type = source.GetType(err);
  if (err.Fail()) return std::string();

  // No source
  if (source_type > v8()->types()->kFirstNonstringType) {
    err = Error::Failure("No source, source_type=%" PRId64, source_type);
    return std::string();
  }

  String str(source);
  std::string source_str = str.ToString(err);

  int64_t start_pos = info.StartPosition(err);

  if (err.Fail()) {
    return std::string();
  }

  int64_t end_pos = info.EndPosition(err);

  if (err.Fail()) {
    return std::string();
  }

  int64_t source_len = source_str.length();

  if (end_pos > source_len) {
    end_pos = source_len;
  }
  int64_t len = end_pos - start_pos;

  // Make sure the substr isn't out of range
  if (start_pos < 0 || len < 0 || start_pos + len > source_len) {
    err = Error::Failure("Invalid source range, start_pos=%" PRId64
                         ", len=%" PRId64 ", source_len=%" PRId64,
                         start_pos, len, source_len);
    return std::string();
  }

  std::string res = source_str.substr(start_pos, len);
  return res;
}


std::string SharedFunctionInfo::ProperName(Error& err) {
  RETURN_IF_THIS_INVALID(std::string());

  String name = Name(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (err.Fail() || res.empty()) {
    Value inferred = GetInferredName(err);
    if (err.Fail() || !inferred.Check()) return std::string();

    // Function may not have inferred name
    if (!inferred.IsHoleOrUndefined(err) && !err.Fail())
      res = inferred.ToString(err);
    if (err.Fail()) return std::string();
  }

  if (res.empty()) res = "(anonymous)";

  return res;
}


std::string SharedFunctionInfo::GetPostfix(Error& err) {
  RETURN_IF_THIS_INVALID(std::string());

  Script script = GetScript(err);
  if (err.Fail() || !script.Check()) return std::string();

  // There is no `Script` for functions created in C++ (and possibly others)
  int64_t type = script.GetType(err);
  if (err.Fail() || type != v8()->types()->kScriptType)
    return std::string("(no script)");

  String name = script.Name(err);
  if (err.Fail()) return std::string();

  int64_t start_pos = StartPosition(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (res.empty()) res = "(no script)";

  int64_t line = 0;
  int64_t column = 0;
  script.GetLineColumnFromPos(start_pos, line, column, err);
  if (err.Fail()) return std::string();

  // NOTE: lines start from 1 in most of editors
  char tmp[128];
  snprintf(tmp, sizeof(tmp), ":%d:%d", static_cast<int>(line + 1),
           static_cast<int>(column));
  return res + tmp;
}

std::string SharedFunctionInfo::ToString(Error& err) {
  std::string res = ProperName(err);
  if (err.Fail()) return std::string();

  return res + " at " + GetPostfix(err);
}


// return end_char+1, which may be less than line_limit if source
// ends before end_inclusive
void Script::GetLines(uint64_t start_line, std::string lines[],
                      uint64_t line_limit, uint32_t& lines_found, Error& err) {
  lines_found = 0;

  HeapObject source = Source(err);
  if (err.Fail()) return;

  int64_t type = source.GetType(err);
  if (err.Fail()) return;

  // No source
  if (type > v8()->types()->kFirstNonstringType) {
    err = Error::Failure("No source, source_type=%" PRId64, type);
    return;
  }

  String str(source);
  std::string source_str = str.ToString(err);
  uint64_t limit = source_str.length();

  uint64_t length = 0;
  uint64_t line_i = 0;
  uint64_t i = 0;
  for (; i < limit && lines_found < line_limit; i++) {
    // \r\n should ski adding a line and column on \r
    if (source_str[i] == '\r' && i < limit - 1 && source_str[i + 1] == '\n') {
      i++;
    }
    if (source_str[i] == '\n' || source_str[i] == '\r') {
      if (line_i >= start_line) {
        lines[lines_found] = std::string(source_str, i - length, length);
        lines_found++;
      }
      line_i++;
      length = 0;
    } else {
      length++;
    }
  }
  if (line_i >= start_line && length != 0 && lines_found < line_limit) {
    lines[lines_found] = std::string(source_str, limit - length, length);
    lines_found++;
  }
}


void Script::GetLineColumnFromPos(int64_t pos, int64_t& line, int64_t& column,
                                  Error& err) {
  line = 0;
  column = 0;

  String source = Source(err);
  if (err.Fail()) return;

  int64_t type = source.GetType(err);
  if (err.Fail()) return;

  // No source
  if (type > v8()->types()->kFirstNonstringType) {
    err = Error(true, "No source");
    return;
  }

  std::string source_str = source.ToString(err);
  int64_t limit = source_str.length();
  if (limit > pos) limit = pos;

  for (int64_t i = 0; i < limit; i++, column++) {
    // \r\n should ski adding a line and column on \r
    if (source_str[i] == '\r' && i < limit - 1 && source_str[i + 1] == '\n') {
      i++;
    }
    if (source_str[i] == '\n' || source_str[i] == '\r') {
      column = 0;
      line++;
    }
  }
}

bool Value::IsHoleOrUndefined(Error& err) {
  HeapObject obj(this);
  if (!obj.Check()) return false;

  int64_t type = obj.GetType(err);
  if (err.Fail()) return false;

  if (type != v8()->types()->kOddballType) return false;

  Oddball odd(this);
  return odd.IsHoleOrUndefined(err);
}


// TODO(indutny): deduplicate this?
bool Value::IsHole(Error& err) {
  HeapObject obj(this);
  if (!obj.Check()) return false;

  int64_t type = obj.GetType(err);
  if (err.Fail()) return false;

  if (type != v8()->types()->kOddballType) return false;

  Oddball odd(this);
  return odd.IsHole(err);
}


std::string Value::GetTypeName(Error& err) {
  Smi smi(this);
  if (smi.Check()) return "(Smi)";

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.GetTypeName(err);
}


std::string Value::ToString(Error& err) {
  RETURN_IF_THIS_INVALID(std::string());

  Smi smi(this);
  if (smi.Check()) return smi.ToString(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.ToString(err);
}


std::string HeapObject::ToString(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return std::string();

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    return n.ToString(false, err);
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    return str.ToString(err);
  }

  if (type == v8()->types()->kSymbolType) {
    Symbol symbol(this);
    return symbol.ToString(err);
  }

  return "<non-string>";
}


std::string Smi::ToString(Error& err) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", static_cast<int>(GetValue()));
  err = Error::Ok();
  return buf;
}


/* Utility function to generate short type names for objects.
 */
std::string HeapObject::GetTypeName(Error& err) {
  int64_t type = GetType(err);
  if (type == v8()->types()->kGlobalObjectType) return "(Global)";
  if (type == v8()->types()->kGlobalProxyType) return "(Global proxy)";
  if (type == v8()->types()->kCodeType) return "(Code)";
  if (type == v8()->types()->kMapType) {
    return "(Map)";
  }
  if (type >= v8()->types()->kFirstContextType &&
      type <= v8()->types()->kLastContextType) {
    return "Context";
  }

  if (JSObject::IsObjectType(v8(), type)) {
    v8::HeapObject map_obj = GetMap(err);
    if (err.Fail()) {
      return std::string();
    }

    v8::Map map(map_obj);
    v8::HeapObject constructor_obj = map.Constructor(err);
    if (err.Fail()) {
      return std::string();
    }

    int64_t constructor_type = constructor_obj.GetType(err);
    if (err.Fail()) {
      return std::string();
    }

    if (constructor_type != v8()->types()->kJSFunctionType) {
      return "(Object)";
    }

    v8::JSFunction constructor(constructor_obj);

    return constructor.Name(err);
  }

  if (type == v8()->types()->kHeapNumberType) {
    return "(HeapNumber)";
  }

  if (type == v8()->types()->kJSArrayType) {
    return "(Array)";
  }

  if (type == v8()->types()->kOddballType) {
    return "(Oddball)";
  }

  if (type == v8()->types()->kJSFunctionType) {
    return "(Function)";
  }

  if (type == *v8()->types()->kJSRegExpType) {
    return "(RegExp)";
  }

  if (type < v8()->types()->kFirstNonstringType) {
    return "(String)";
  }

  if (type == v8()->types()->kFixedArrayType) {
    return "(FixedArray)";
  }

  if (type == v8()->types()->kJSArrayBufferType) {
    return "(ArrayBuffer)";
  }

  if (type == v8()->types()->kJSTypedArrayType) {
    return "(ArrayBufferView)";
  }

  if (type == v8()->types()->kJSDateType) {
    return "(Date)";
  }

  std::string unknown("unknown: ");

  return unknown + std::to_string(type);
}


std::string HeapNumber::ToString(bool whole, Error& err) {
  char buf[128];
  const char* fmt = whole ? "%f" : "%.2f";
  auto val = GetValue(err);
  if (err.Fail() || !val.Check()) {
    err = Error::Ok();
    return "???";
  }
  snprintf(buf, sizeof(buf), fmt, val);
  return buf;
}

std::string JSDate::ToString(Error& err) {
  v8::Value val(GetValue(err));

  // Check if it is SMI
  // e.g: date = new Date(1)
  v8::Smi smi(val);
  if (smi.Check()) {
    std::string s = smi.ToString(err);
    if (err.Fail()) return "";
    return s;
  }

  // Check if it is HeapNumber
  // e.g: date = new Date()
  v8::HeapNumber hn(val);
  if (hn.Check()) {
    std::string s = hn.ToString(true, err);
    if (err.Fail()) return "";
    return s;
  }

  PRINT_DEBUG("JSDate is not a Smi neither a HeapNumber");
  return "";
}


std::string Symbol::ToString(Error& err) {
  if (!String::IsString(v8(), Name(err), err)) {
    return "Symbol()";
  }
  HeapObject name = Name(err);
  // Use \? so we don't treat this as a trigraph.
  RETURN_IF_INVALID(name, "Symbol(\?\?\?)");
  return "Symbol('" + String(name).ToString(err) + "')";
}


std::string String::ToString(Error& err) {
  CheckedType<int64_t> repr = Representation(err);
  RETURN_IF_INVALID(repr, std::string());

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (*repr == v8()->string()->kSeqStringTag) {
    if (encoding == v8()->string()->kOneByteStringTag) {
      OneByteString one(this);
      return one.ToString(err);
    } else if (encoding == v8()->string()->kTwoByteStringTag) {
      TwoByteString two(this);
      return two.ToString(err);
    }

    err = Error::Failure("Unsupported seq string encoding %" PRId64, encoding);
    return std::string();
  }

  if (*repr == v8()->string()->kConsStringTag) {
    ConsString cons(this);
    return cons.ToString(err);
  }

  if (*repr == v8()->string()->kSlicedStringTag) {
    SlicedString sliced(this);
    return sliced.ToString(err);
  }

  // TODO(indutny): add support for external strings
  if (*repr == v8()->string()->kExternalStringTag) {
    return std::string("(external)");
  }

  if (*repr == v8()->string()->kThinStringTag) {
    ThinString thin(this);
    return thin.ToString(err);
  }

  err = Error::Failure("Unsupported string representation %" PRId64, *repr);
  return std::string();
}


// Context locals iterator implementations
Context::Locals::Locals(Context* context, Error& err) {
  context_ = context;
  HeapObject scope_obj = context_->GetScopeInfo(err);
  if (err.Fail()) return;

  scope_info_ = ScopeInfo(scope_obj);
  Smi local_count_smi = scope_info_.ContextLocalCount(err);
  if (err.Fail()) return;

  local_count_ = local_count_smi.GetValue();
}

Context::Locals::Iterator Context::Locals::begin() { return Iterator(0, this); }

Context::Locals::Iterator Context::Locals::end() {
  return Iterator(local_count_, this);
}

const Context::Locals::Iterator Context::Locals::Iterator::operator++(int) {
  current_++;
  return Iterator(current_, this->outer_);
}

bool Context::Locals::Iterator::operator!=(Context::Locals::Iterator that) {
  return current_ != that.current_ || outer_->context_ != that.outer_->context_;
}

v8::Value Context::Locals::Iterator::operator*() {
  Error err;
  return outer_->context_->ContextSlot(current_, err);
}

String Context::Locals::Iterator::LocalName(Error& err) {
  return outer_->scope_info_.ContextLocalName(current_, err);
}

Value Context::Locals::Iterator::GetValue(Error& err) {
  return outer_->context_->ContextSlot(current_, err);
}


HeapObject Map::Constructor(Error& err) {
  Map current = this;

  do {
    HeapObject obj = current.MaybeConstructor(err);
    if (err.Fail()) return current;

    int64_t type = obj.GetType(err);
    if (err.Fail()) return current;

    current = obj;
    if (type != v8()->types()->kMapType) break;
  } while (true);

  return current;
}

/* Returns the set of keys on an object - similar to Object.keys(obj) in
 * Javascript. That includes array indices but not special fields like
 * "length" on an array.
 */
void JSObject::Keys(std::vector<std::string>& keys, Error& err) {
  keys.clear();

  // First handle array indices.
  ElementKeys(keys, err);

  HeapObject map_obj = GetMap(err);

  Map map(map_obj);

  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return;

  if (is_dict) {
    DictionaryKeys(keys, err);
  } else {
    DescriptorKeys(keys, map, err);
  }

  return;
}


std::vector<std::pair<Value, Value>> JSObject::Entries(Error& err) {
  HeapObject map_obj = GetMap(err);

  Map map(map_obj);
  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return {};

  if (is_dict) {
    return DictionaryEntries(err);
  } else {
    return DescriptorEntries(map, err);
  }
}


std::vector<std::pair<Value, Value>> JSObject::DictionaryEntries(Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return {};

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return {};

  std::vector<std::pair<Value, Value>> entries;
  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);

    if (err.Fail()) return entries;

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return entries;
    if (is_hole) continue;

    Value value = dictionary.GetValue(i, err);

    entries.push_back(std::pair<Value, Value>(key, value));
  }
  return entries;
}


std::vector<std::pair<Value, Value>> JSObject::DescriptorEntries(Map map,
                                                                 Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  RETURN_IF_INVALID(descriptors_obj, {});

  DescriptorArray descriptors(descriptors_obj);

  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return {};

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return {};

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return {};

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return {};

  FixedArray extra_properties(extra_properties_obj);

  std::vector<std::pair<Value, Value>> entries;
  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i);
    if (!details.Check()) {
      PRINT_DEBUG("Failed to get details for index %ld", i);
      entries.push_back(std::pair<Value, Value>(Value(), Value()));
      continue;
    }

    Value key = descriptors.GetKey(i);
    if (!key.Check()) continue;

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i);
      if (!value.Check()) continue;

      entries.push_back(std::pair<Value, Value>(key, value));
      continue;
    }

    // Skip non-fields for now, Object.keys(obj) does
    // not seem to return these (for example the "length"
    // field on an array).
    if (!descriptors.IsFieldDetails(details)) continue;

    if (descriptors.IsDoubleField(details)) continue;

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    Value value;
    if (index < 0) {
      value = GetInObjectValue<Value>(instance_size, index, err);
    } else {
      value = extra_properties.Get<Value>(index, err);
    }

    entries.push_back(std::pair<Value, Value>(key, value));
  }

  return entries;
}


void JSObject::ElementKeys(std::vector<std::string>& keys, Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return;

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return;

  int64_t length = length_smi.GetValue();
  for (int i = 0; i < length; ++i) {
    // Add keys for anything that isn't a hole.
    Value value = elements.Get<Value>(i, err);
    if (err.Fail()) continue;
    ;

    bool is_hole = value.IsHole(err);
    if (err.Fail()) continue;
    if (!is_hole) {
      keys.push_back(std::to_string(i));
    }
  }
}

void JSObject::DictionaryKeys(std::vector<std::string>& keys, Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return;

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return;

  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return;

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return;
    if (is_hole) continue;

    std::string key_name = key.ToString(err);
    if (err.Fail()) {
      // TODO - should I continue onto the next key here instead.
      return;
    }

    keys.push_back(key_name);
  }
}

void JSObject::DescriptorKeys(std::vector<std::string>& keys, Map map,
                              Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  RETURN_IF_INVALID(descriptors_obj, );

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return;

  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i);
    if (!details.Check()) {
      PRINT_DEBUG("Failed to get details for index %ld", i);
      keys.push_back("???");
      continue;
    }

    Value key = descriptors.GetKey(i);
    RETURN_IF_INVALID(key, );

    // Skip non-fields for now, Object.keys(obj) does
    // not seem to return these (for example the "length"
    // field on an array).
    if (!descriptors.IsFieldDetails(details)) {
      continue;
    }

    std::string key_name = key.ToString(err);
    if (err.Fail()) {
      // TODO - should I continue onto the next key here instead.
      return;
    }

    keys.push_back(key_name);
  }
}

/* Return the v8 value for a property stored using the given key.
 * (Caller should have some idea of what type of object will be stored
 * in that key, they will get a v8::Value back that they can cast.)
 */
Value JSObject::GetProperty(std::string key_name, Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) Value();

  Map map(map_obj);

  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return Value();

  if (is_dict) {
    return GetDictionaryProperty(key_name, err);
  } else {
    return GetDescriptorProperty(key_name, map, err);
  }

  if (err.Fail()) return Value();

  // Nothing's gone wrong, we just didn't find the key.
  return Value();
}

Value JSObject::GetDictionaryProperty(std::string key_name, Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return Value();

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return Value();

  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return Value();

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return Value();
    if (is_hole) continue;

    if (key.ToString(err) == key_name) {
      Value value = dictionary.GetValue(i, err);

      if (err.Fail()) return Value();

      return value;
    }
  }
  return Value();
}

Value JSObject::GetDescriptorProperty(std::string key_name, Map map,
                                      Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  RETURN_IF_INVALID(descriptors_obj, Value());

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return Value();

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return Value();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return Value();

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return Value();

  FixedArray extra_properties(extra_properties_obj);

  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i);
    if (!details.Check()) {
      PRINT_DEBUG("Failed to get details for index %ld", i);
      continue;
    }

    Value key = descriptors.GetKey(i);
    RETURN_IF_INVALID(key, Value());

    if (key.ToString(err) != key_name) {
      continue;
    }

    // Found the right key, get the value.
    if (err.Fail()) return Value();

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i);
      RETURN_IF_INVALID(value, Value());

      continue;
    }

    // Skip non-fields for now
    if (!descriptors.IsFieldDetails(details)) {
      // This path would return the length field for an array,
      // however Object.keys(arr) doesn't return length as a
      // field so neither do we.
      continue;
    }

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      return GetDoubleField(index, err);
    } else {
      Value value;
      if (index < 0) {
        value = GetInObjectValue<Value>(instance_size, index, err);
      } else {
        value = extra_properties.Get<Value>(index, err);
      }

      if (err.Fail()) {
        return Value();
      } else {
        return value;
      };
    }
    if (err.Fail()) return Value();
  }
  return Value();
}


/* An array is also an object so this method is on JSObject
 * not JSArray.
 */
int64_t JSObject::GetArrayLength(Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return 0;

  FixedArray elements(elements_obj);
  Smi length_smi = elements.Length(err);
  if (err.Fail()) return 0;

  int64_t length = length_smi.GetValue();
  return length;
}


/* An array is also an object so this method is on JSObject
 * not JSArray.
 * Note that you the user should know what the expect the array to contain
 * and should check they haven't been returned a hole.
 */
v8::Value JSObject::GetArrayElement(int64_t pos, Error& err) {
  if (pos < 0) {
    // TODO - Set err.Fail()?
    return Value();
  }

  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return Value();

  FixedArray elements(elements_obj);
  Smi length_smi = elements.Length(err);
  if (err.Fail()) return Value();

  int64_t length = length_smi.GetValue();
  if (pos >= length) {
    return Value();
  }

  Value value = elements.Get<v8::Value>(pos, err);
  if (err.Fail()) return Value();

  return value;
}


bool JSError::HasStackTrace(Error& err) {
  StackTrace stack_trace = GetStackTrace(err);

  return stack_trace.GetFrameCount() > -1;
}


JSArray JSError::GetFrameArray(Error& err) {
  RETURN_IF_THIS_INVALID(JSArray());

  v8::Value maybe_stack = GetProperty(stack_trace_property(), err);

  if (err.Fail() || !maybe_stack.Check()) {
    PRINT_DEBUG("Couldn't find a symbol property in the Error object.");
    return JSArray();
  }

  int64_t type = v8::HeapObject(maybe_stack).GetType(err);

  if (err.Fail()) {
    PRINT_DEBUG("Symbol property references an invalid object.");
    return JSArray();
  }

  // NOTE (mmarchini): The stack is stored as a JSArray
  if (type != v8()->types()->kJSArrayType) {
    PRINT_DEBUG("Symbol property doesn't have the right type.");
    return JSArray();
  }

  v8::JSArray arr(maybe_stack);

  return arr;
}


StackTrace::Iterator StackTrace::begin() {
  Error err;
  return StackTrace::Iterator(this, err);
}

StackTrace::Iterator StackTrace::end() {
  Error err;
  return StackTrace::Iterator(this, GetFrameCount(), err);
}

StackTrace::StackTrace(JSArray frame_array, Error& err)
    : frame_array_(frame_array) {
  if (!frame_array.Check()) {
    PRINT_DEBUG("JS Array is not a valid object");
    len_ = -1;
    multiplier_ = -1;
    return;
  }

  v8::Value maybe_stack_len = frame_array.GetArrayElement(0, err);

  if (err.Fail()) {
    PRINT_DEBUG("Couldn't get the first element from the stack array");
    return;
  }

  len_ = v8::Smi(maybe_stack_len).GetValue();

  multiplier_ = 5;
  // On Node.js v8.x, the first array element is the stack size, and each
  // stack frame use 5 elements.
  if ((len_ * multiplier_ + 1) != frame_array_.GetArrayLength(err)) {
    // On Node.js v6.x, the first array element is zero, and each stack frame
    // use 4 element.
    multiplier_ = 4;
    if ((len_ != 0) ||
        ((frame_array_.GetArrayLength(err) - 1) % multiplier_ != 0)) {
      PRINT_DEBUG(
          "JSArray doesn't look like a Stack Frames array. stack_len: %d "
          "array_len: %ld",
          len_, frame_array_.GetArrayLength(err));
      len_ = -1;
      multiplier_ = -1;
      return;
    }
    len_ = (frame_array_.GetArrayLength(err) - 1) / multiplier_;
  }
}


StackTrace JSError::GetStackTrace(Error& err) {
  return StackTrace(GetFrameArray(err), err);
}


StackFrame StackTrace::GetFrame(uint32_t index, Error& err) {
  return StackFrame(this, index);
}

StackFrame::StackFrame(StackTrace* stack_trace, int index)
    : stack_trace_(stack_trace), index_(index) {}

JSFunction StackFrame::GetFunction(Error& err) {
  JSArray frame_array = stack_trace_->frame_array_;
  v8::Value maybe_fn = frame_array.GetArrayElement(frame_array_index(), err);
  if (err.Fail()) return JSFunction();
  return JSFunction(maybe_fn);
}

int StackFrame::frame_array_index() {
  int multiplier = stack_trace_->multiplier_;
  const int js_function_pos = 1;
  const int begin_offset = 1;
  return begin_offset + js_function_pos + (index_ * multiplier);
}


}  // namespace v8
}  // namespace llnode
