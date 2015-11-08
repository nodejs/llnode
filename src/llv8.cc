#include <assert.h>
#include <string.h>

#include "llv8.h"
#include "llv8-inl.h"

namespace llnode {
namespace v8 {

using namespace lldb;

static std::string kConstantPrefix = "v8dbg_";

void LLV8::Load(SBTarget target) {
  // No need to reload
  if (target_ == target)
    return;

  target_ = target;
  process_ = target_.GetProcess();

  common.Assign(target);
  smi.Assign(target, &common);
  heap_obj.Assign(target, &common);
  map.Assign(target, &common);
  js_object.Assign(target, &common);
  heap_number.Assign(target, &common);
  js_array.Assign(target, &common);
  js_function.Assign(target, &common);
  shared_info.Assign(target, &common);
  scope_info.Assign(target, &common);
  context.Assign(target, &common);
  script.Assign(target, &common);
  string.Assign(target, &common);
  one_byte_string.Assign(target, &common);
  two_byte_string.Assign(target, &common);
  cons_string.Assign(target, &common);
  sliced_string.Assign(target, &common);
  fixed_array_base.Assign(target, &common);
  fixed_array.Assign(target, &common);
  oddball.Assign(target, &common);
  js_array_buffer.Assign(target, &common);
  js_array_buffer_view.Assign(target, &common);
  descriptor_array.Assign(target, &common);
  name_dictionary.Assign(target, &common);
  frame.Assign(target, &common);
  types.Assign(target, &common);
}


int64_t LLV8::LoadConstant(const char* name) {
  SBValue v = target_.FindFirstGlobalVariable((kConstantPrefix + name).c_str());

  if (v.GetError().Fail())
    fprintf(stderr, "Failed to load %s\n", name);

  return v.GetValueAsSigned(0);
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


double LLV8::LoadDouble(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
      sizeof(double), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 double value");
    return -1.0;
  }

  err = Error::Ok();
  return *reinterpret_cast<double*>(&value);
}


std::string LLV8::LoadString(int64_t addr, int64_t length, Error& err) {
  char* buf = new char[length + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 one byte string");
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
  char* buf = new char[length * 2 + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length * 2), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 one byte string");
    delete[] buf;
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


uint8_t* LLV8::LoadChunk(int64_t addr, int64_t length, Error& err) {
  uint8_t* buf = new uint8_t[length];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    // TODO(indutny): add more information
    err = Error::Failure("Failed to load V8 memory chunk");
    delete[] buf;
    return nullptr;
  }

  err = Error::Ok();
  return buf;
}


std::string JSFrame::Inspect(bool with_args, Error& err) {
  Value context =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kContextOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_context = Smi(context);
  if (smi_context.Check() &&
      smi_context.GetValue() == v8()->frame()->kAdaptorFrame) {
    return "<adaptor>";
  }

  Value marker =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kMarkerOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_marker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == v8()->frame()->kEntryFrame) {
      return "<entry>";
    } else if (value == v8()->frame()->kEntryConstructFrame) {
      return "<entry_construct>";
    } else if (value == v8()->frame()->kExitFrame) {
      return "<exit>";
    } else if (value == v8()->frame()->kInternalFrame) {
      return "<internal>";
    } else if (value == v8()->frame()->kConstructFrame) {
      return "<constructor>";
    } else if (value != v8()->frame()->kJSFrame &&
               value != v8()->frame()->kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker");
      return std::string();
    }
  }

  // We are dealing with function or internal code (probably stub)
  JSFunction fn =
      v8()->LoadValue<JSFunction>(raw() + v8()->frame()->kFunctionOffset, err);
  if (err.Fail()) return std::string();

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) return std::string();

  if (fn_type == v8()->types()->kCodeType) return "<internal code>";
  if (fn_type != v8()->types()->kJSFunctionType) return "<non-function>";

  std::string args;
  if (with_args) {
    args = InspectArgs(fn, err);
    if (err.Fail()) return std::string();
  }

  char tmp[128];
  snprintf(tmp, sizeof(tmp), " fn=0x%016llx", fn.raw());
  return fn.GetDebugLine(args, err) + tmp;
}


std::string JSFrame::InspectArgs(JSFunction fn, Error& err) {
  SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) return std::string();

  int64_t param_count = info.ParameterCount(err);
  if (err.Fail()) return std::string();

  Value receiver = GetReceiver(param_count, err);
  if (err.Fail()) return std::string();

  std::string res = "this=" + receiver.Inspect(false, err);
  if (err.Fail()) return std::string();

  for (int64_t i = 0; i < param_count; i++) {
    Value param = GetParam(i, param_count, err);
    if (err.Fail()) return std::string();

    res += ", " + param.Inspect(false, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string JSFunction::Name(SharedFunctionInfo info, Error& err) {
  String name = info.Name(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (err.Fail() || res.empty()) {
    name = info.InferredName(err);
    if (err.Fail()) return std::string();

    res = name.ToString(err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string JSFunction::GetDebugLine(std::string args, Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return std::string();

  std::string res = Name(info, err);
  if (err.Fail()) return std::string();

  if (res.empty())
    res = "(anonymous)";
  if (!args.empty())
    res += "(" + args + ")";

  res += " at ";

  std::string shared;

  res += info.GetPostfix(err);
  if (err.Fail()) return std::string();

  return res;
}


std::string JSFunction::Inspect(bool detailed, Error& err) {
  std::string res = "<function: " + GetDebugLine(std::string(), err);
  if (err.Fail()) return std::string();

  if (detailed) {
    HeapObject context_obj = GetContext(err);
    if (err.Fail()) return std::string();

    Context context(context_obj);

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\n  context=0x%016llx", context.raw());
    res += tmp;

    std::string context_str = context.Inspect(err);
    if (err.Fail()) return std::string();

    if (!context_str.empty())
      res += "{\n" + context_str + "}";
  }

  return res + ">";
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

  res += script.GetLineColumnFromPos(start_pos, err);
  if (err.Fail()) return std::string();

  return res;
}


std::string Script::GetLineColumnFromPos(int64_t pos, Error& err) {
  char tmp[128];

  HeapObject source = Source(err);
  if (err.Fail()) return std::string();

  int64_t type = source.GetType(err);
  if (err.Fail()) return std::string();

  // No source
  if (type > v8()->types()->kFirstNonstringType) {
    snprintf(tmp, sizeof(tmp), ":0:%d", static_cast<int>(pos));
    return tmp;
  }

  String str(source);
  std::string source_str = str.ToString(err);
  int64_t limit = source_str.length();
  if (limit > pos)
    limit = pos;

  int line = 1;
  int column = 1;
  for (int64_t i = 0; i < limit; i++, column++) {
    if (source_str[i] == '\n' || source_str[i] == '\r') {
      column = 0;
      line++;
    }
  }

  snprintf(tmp, sizeof(tmp), ":%d:%d", static_cast<int>(line),
      static_cast<int>(column));
  return tmp;
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


std::string Value::Inspect(bool detailed, Error& err) {
  Smi smi(this);
  if (smi.Check())
    return smi.Inspect(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.Inspect(detailed, err);
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

  return obj.ToString(err);
}


std::string HeapObject::ToString(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return std::string();

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    return n.ToString(err);
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    return str.ToString(err);
  }

  return "<non-string>";
}


std::string HeapObject::Inspect(bool detailed, Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return std::string();

  // TODO(indutny): make this configurable
  char buf[32];
  snprintf(buf, sizeof(buf), "0x%016llx:", raw());
  std::string pre = buf;

  if (type == v8()->types()->kGlobalObjectType) return pre + "<Global>";
  if (type == v8()->types()->kCodeType) return pre + "<Code>";

  if (type == v8()->types()->kJSObjectType) {
    JSObject o(this);
    return pre + o.Inspect(detailed, err);
  }

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    return pre + n.Inspect(err);
  }

  if (type == v8()->types()->kJSArrayType) {
    JSArray arr(this);
    return pre + arr.Inspect(detailed, err);
  }

  if (type == v8()->types()->kOddballType) {
    Oddball o(this);
    return pre + o.Inspect(err);
  }

  if (type == v8()->types()->kJSFunctionType) {
    JSFunction fn(this);
    return pre + fn.Inspect(detailed, err);
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    return pre + str.Inspect(err);
  }

  if (type == v8()->types()->kFixedArrayType) {
    FixedArray arr(this);
    return pre + arr.Inspect(err);
  }

  if (type == v8()->types()->kJSArrayBufferType) {
    JSArrayBuffer buf(this);
    return pre + buf.Inspect(err);
  }

  if (type == v8()->types()->kJSTypedArrayType) {
    JSArrayBufferView view(this);
    return pre + view.Inspect(err);
  }

  return pre + "<unknown>";
}


std::string Smi::ToString(Error& err) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", static_cast<int>(GetValue()));
  err = Error::Ok();
  return buf;
}


std::string Smi::Inspect(Error& err) {
  return "<Smi: " + ToString(err) + ">";
}


std::string HeapNumber::ToString(Error& err) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%f", GetValue(err));
  err = Error::Ok();
  return buf;
}


std::string HeapNumber::Inspect(Error& err) {
  return "<Number: " + ToString(err) + ">";
}


std::string String::ToString(Error& err) {
  int64_t repr = Representation(err);
  if (err.Fail()) return std::string();

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (repr == v8()->string()->kSeqStringTag) {
    if (encoding == v8()->string()->kOneByteStringTag) {
      OneByteString one(this);
      return one.ToString(err);
    } else if (encoding == v8()->string()->kTwoByteStringTag) {
      TwoByteString two(this);
      return two.ToString(err);
    }

    err = Error::Failure("Unsupported seq string encoding");
    return std::string();
  }

  if (repr == v8()->string()->kConsStringTag) {
    ConsString cons(this);
    return cons.ToString(err);
  }

  if (repr == v8()->string()->kSlicedStringTag) {
    SlicedString sliced(this);
    return sliced.ToString(err);
  }

  err = Error::Failure("Unsupported string representation");
  return std::string();
}


std::string String::Inspect(Error& err) {
  std::string val = ToString(err);
  if (err.Fail()) return std::string();

  // TODO(indutny): add length
  if (val.length() > kInspectSize)
    val = val.substr(0, kInspectSize) + "...";

  return "<String: \"" + val + "\">";
}


std::string FixedArray::Inspect(Error& err) {
  Smi length = Length(err);
  if (err.Fail()) return std::string();
  return "<FixedArray, len=" + length.ToString(err) + ">";
}


std::string Context::Inspect(Error& err) {
  std::string res;
  // Not enough postmortem information, return bare minimum
  if (v8()->shared_info()->kScopeInfoOffset == -1) return res;

  JSFunction closure = Closure(err);
  if (err.Fail()) return std::string();

  SharedFunctionInfo info = closure.Info(err);
  if (err.Fail()) return std::string();

  HeapObject scope_obj = info.GetScopeInfo(err);
  if (err.Fail()) return std::string();

  ScopeInfo scope(scope_obj);

  Smi param_count_smi = scope.ParameterCount(err);
  if (err.Fail()) return std::string();
  Smi stack_count_smi = scope.StackLocalCount(err);
  if (err.Fail()) return std::string();
  Smi local_count_smi = scope.ContextLocalCount(err);
  if (err.Fail()) return std::string();

  int param_count = param_count_smi.GetValue();
  int stack_count = stack_count_smi.GetValue();
  int local_count = local_count_smi.GetValue();
  for (int i = 0; i < local_count; i++) {
    String name = scope.ContextLocalName(i, param_count, stack_count, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    " + name.ToString(err) + "=";
    if (err.Fail()) return std::string();

    Value value = ContextSlot(i, err);
    if (err.Fail()) return std::string();

    res += value.Inspect(false, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string Oddball::Inspect(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return std::string();

  int64_t kind_val = kind.GetValue();
  if (kind_val == v8()->oddball()->kException) return "<exception>";
  if (kind_val == v8()->oddball()->kFalse) return "<false>";
  if (kind_val == v8()->oddball()->kTrue) return "<true>";
  if (kind_val == v8()->oddball()->kUndefined) return "<undefined>";
  if (kind_val == v8()->oddball()->kNull) return "<null>";
  if (kind_val == v8()->oddball()->kTheHole) return "<hole>";
  if (kind_val == v8()->oddball()->kUninitialized) return "<uninitialized>";
  return "<Oddball>";
}


std::string JSArrayBuffer::Inspect(Error& err) {
  bool neutered = WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) return "<ArrayBuffer [neutered]>";

  int64_t data = BackingStore(err);
  if (err.Fail()) return std::string();

  Smi length = ByteLength(err);
  if (err.Fail()) return std::string();

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "<ArrayBuffer 0x%016llx:%d>", data,
      static_cast<int>(length.GetValue()));
  return tmp;
}


std::string JSArrayBufferView::Inspect(Error& err) {
  JSArrayBuffer buf = Buffer(err);
  if (err.Fail()) return std::string();

  bool neutered = buf.WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) return "<ArrayBufferView [neutered]>";

  int64_t data = buf.BackingStore(err);
  if (err.Fail()) return std::string();

  Smi off = ByteOffset(err);
  if (err.Fail()) return std::string();

  Smi length = ByteLength(err);
  if (err.Fail()) return std::string();

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "<ArrayBufferView 0x%016llx+%d:%d>", data,
      static_cast<int>(off.GetValue()), static_cast<int>(length.GetValue()));
  return tmp;
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


std::string JSObject::Inspect(bool detailed, Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);
  HeapObject constructor_obj = map.Constructor(err);
  if (err.Fail()) return std::string();

  int64_t constructor_type = constructor_obj.GetType(err);
  if (err.Fail()) return std::string();

  if (constructor_type != v8()->types()->kJSFunctionType)
    return "<Object: no constructor>";

  JSFunction constructor(constructor_obj);

  std::string res = "<Object: " + constructor.Name(err);
  if (err.Fail()) return std::string();

  // Print properties in detailed mode
  if (detailed) {
    res += " " + InspectProperties(err);
    if (err.Fail()) return std::string();
  }

  res += ">";
  return res;
}


std::string JSObject::InspectProperties(Error& err) {
  std::string res;

  std::string elems = InspectElements(err);
  if (err.Fail()) return std::string();

  if (!elems.empty()) res = "elements {\n" + elems + "}";

  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);

  std::string props;
  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return std::string();

  if (is_dict)
    props = InspectDictionary(err);
  else
    props = InspectDescriptors(map, err);

  if (err.Fail()) return std::string();

  if (!props.empty()) {
    if (!res.empty())
      res += "\n  ";
    res += "properties {\n" + props + "}";
  }

  return res;
}


std::string JSObject::InspectElements(Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return std::string();

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return std::string();

  int64_t length = length_smi.GetValue();
  std::string res;
  for (int64_t i = 0; i < length; i++) {
    Value value = elements.Get<Value>(i, err);
    if (err.Fail()) return std::string();

    bool is_hole = value.IsHole(err);
    if (err.Fail()) return std::string();

    // Skip holes
    if (is_hole) continue;

    if (!res.empty()) res += ",\n";

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "    [%d]=", static_cast<int>(i));
    res += tmp;

    res += value.Inspect(false, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string JSObject::InspectDictionary(Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return std::string();

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return std::string();

  std::string res;
  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return std::string();

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return std::string();
    if (is_hole) continue;

    Value value = dictionary.GetValue(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    ." + key.ToString(err) + "=";
    if (err.Fail()) return std::string();

    res += value.Inspect(false, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string JSObject::InspectDescriptors(Map map, Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return std::string();

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return std::string();

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return std::string();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return std::string();

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return std::string();

  FixedArray extra_properties(extra_properties_obj);

  std::string res;
  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) return std::string();

    // Skip non-fields for now
    if (!descriptors.IsFieldDetails(details)) continue;

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    ." + key.ToString(err) + "=";
    if (err.Fail()) return std::string();

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      double value;
      if (index < 0)
        value = GetInObjectValue<double>(instance_size, index, err);
      else
        value = extra_properties.Get<double>(index, err);

      if (err.Fail()) return std::string();

      char tmp[100];
      snprintf(tmp, sizeof(tmp), "%f", value);
      res += tmp;
    } else {
      Value value;
      if (index < 0)
        value = GetInObjectValue<Value>(instance_size, index, err);
      else
        value = extra_properties.Get<Value>(index, err);

      if (err.Fail()) return std::string();

      res += value.Inspect(false, err);
    }
    if (err.Fail()) return std::string();
  }

  return res;
}


template <class T>
T JSObject::GetInObjectValue(int64_t size, int index, Error& err) {
  return LoadFieldValue<T>(size + index * v8()->common()->kPointerSize,
      err);
}


std::string JSArray::Inspect(bool detailed, Error& err) {
  Smi length = Length(err);
  if (err.Fail()) return std::string();

  std::string res = "<Array: length=" + length.ToString(err);
  if (detailed) {
    std::string elems = InspectElements(err);
    if (err.Fail()) return std::string();

    if (!elems.empty()) res += " {\n" + elems + "}";
  }

  return res + ">";
}

}  // namespace v8
}  // namespace llnode
