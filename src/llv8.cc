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

  kPointerSize = 1 << LoadConstant("PointerSizeLog2");

  smi_.kTag = LoadConstant("SmiTag");
  smi_.kTagMask = LoadConstant("SmiTagMask");
  smi_.kShiftSize = LoadConstant("SmiShiftSize");

  heap_obj_.kTag = LoadConstant("HeapObjectTag");
  heap_obj_.kTagMask = LoadConstant("HeapObjectTagMask");
  heap_obj_.kMapOffset = LoadConstant("class_HeapObject__map__Map");

  map_.kInstanceAttrsOffset =
      LoadConstant("class_Map__instance_attributes__int");
  map_.kMaybeConstructorOffset =
      LoadConstant("class_Map__constructor_or_backpointer__Object");
  map_.kInstanceDescriptorsOffset =
      LoadConstant("class_Map__instance_descriptors__DescriptorArray");
  map_.kBitField3Offset = LoadConstant("class_Map__bit_field3__int");
  map_.kInObjectPropertiesOffset = LoadConstant(
      "class_Map__inobject_properties_or_constructor_function_index__int");
  map_.kInstanceSizeOffset = LoadConstant("class_Map__instance_size__int");

  map_.kDictionaryMapShift = LoadConstant("bit_field3_dictionary_map_shift");

  js_object_.kPropertiesOffset =
      LoadConstant("class_JSObject__properties__FixedArray");
  js_object_.kElementsOffset = LoadConstant("class_JSObject__elements__Object");

  js_array_.kLengthOffset =
      LoadConstant("class_JSArray__length__Object");

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
  shared_info_.kParameterCountOffset = LoadConstant(
      "class_SharedFunctionInfo__internal_formal_parameter_count__SMI");

  // TODO(indutny): move it to post-mortem
  shared_info_.kStartPositionShift = 2;

  script_.kNameOffset = LoadConstant("class_Script__name__Object");
  script_.kLineOffsetOffset = LoadConstant("class_Script__line_offset__SMI");
  script_.kSourceOffset = LoadConstant("class_Script__source__Object");
  script_.kLineEndsOffset = LoadConstant("class_Script__line_ends__Object");

  string_.kEncodingMask = LoadConstant("StringEncodingMask");
  string_.kRepresentationMask = LoadConstant("StringRepresentationMask");

  string_.kOneByteStringTag = LoadConstant("OneByteStringTag");
  string_.kTwoByteStringTag = LoadConstant("TwoByteStringTag");
  string_.kSeqStringTag = LoadConstant("SeqStringTag");
  string_.kConsStringTag = LoadConstant("ConsStringTag");
  string_.kSlicedStringTag = LoadConstant("SlicedStringTag");
  string_.kExternalStringTag = LoadConstant("ExternalStringTag");

  string_.kLengthOffset = LoadConstant("class_String__length__SMI");

  one_byte_string_.kCharsOffset =
      LoadConstant("class_SeqOneByteString__chars__char");

  two_byte_string_.kCharsOffset =
      LoadConstant("class_SeqTwoByteString__chars__char");

  cons_string_.kFirstOffset = LoadConstant("class_ConsString__first__String");
  cons_string_.kSecondOffset = LoadConstant("class_ConsString__second__String");

  sliced_string_.kParentOffset =
      LoadConstant("class_SlicedString__parent__String");
  sliced_string_.kOffsetOffset =
      LoadConstant("class_SlicedString__offset__SMI");

  fixed_array_base_.kLengthOffset =
      LoadConstant("class_FixedArrayBase__length__SMI");

  fixed_array_.kDataOffset = LoadConstant("class_FixedArray__data__uintptr_t");

  oddball_.kKindOffset = LoadConstant("class_Oddball__kind_offset__int");

  oddball_.kException = LoadConstant("OddballException");
  oddball_.kFalse = LoadConstant("OddballFalse");
  oddball_.kTrue = LoadConstant("OddballTrue");
  oddball_.kUndefined = LoadConstant("OddballUndefined");
  oddball_.kTheHole = LoadConstant("OddballTheHole");
  oddball_.kNull = LoadConstant("OddballNull");
  oddball_.kUninitialized = LoadConstant("OddballUninitialized");

  js_array_buffer_.kBackingStoreOffset =
    LoadConstant("class_JSArrayBuffer__backing_store__Object");
  js_array_buffer_.kByteLengthOffset =
    LoadConstant("class_JSArrayBuffer__byte_length__Object");

  js_array_buffer_view_.kBufferOffset =
    LoadConstant("class_JSArrayBufferView__buffer__Object");
  js_array_buffer_view_.kByteOffsetOffset =
    LoadConstant("class_JSArrayBufferView__byte_offset__Object");
  js_array_buffer_view_.kByteLengthOffset =
    LoadConstant("class_JSArrayBufferView__raw_byte_length__Object");

  descriptor_array_.kDetailsOffset = LoadConstant("prop_desc_details");
  descriptor_array_.kKeyOffset = LoadConstant("prop_desc_key");
  descriptor_array_.kValueOffset = LoadConstant("prop_desc_value");

  descriptor_array_.kPropertyIndexMask = LoadConstant("prop_index_mask");
  descriptor_array_.kPropertyIndexShift = LoadConstant("prop_index_shift");
  descriptor_array_.kPropertyTypeMask = LoadConstant("prop_type_mask");
  descriptor_array_.kFieldType = LoadConstant("prop_type_field");

  descriptor_array_.kFirstIndex = LoadConstant("prop_idx_first");
  descriptor_array_.kSize = LoadConstant("prop_desc_size");

  // TODO(indutny): move this to postmortem
  name_dictionary_.kKeyOffset = 0;
  name_dictionary_.kValueOffset = 1;

  name_dictionary_.kEntrySize =
      LoadConstant("class_NameDictionaryShape__entry_size__int");
  name_dictionary_.kPrefixSize =
      LoadConstant("class_NameDictionaryShape__prefix_size__int");

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

  types_.kMapType = LoadConstant("type_Map__MAP_TYPE");
  types_.kGlobalObjectType =
      LoadConstant("type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE");
  types_.kOddballType = LoadConstant("type_Oddball__ODDBALL_TYPE");
  types_.kJSObjectType = LoadConstant("type_JSObject__JS_OBJECT_TYPE");
  types_.kJSArrayType = LoadConstant("type_JSArray__JS_ARRAY_TYPE");
  types_.kCodeType = LoadConstant("type_Code__CODE_TYPE");
  types_.kJSFunctionType = LoadConstant("type_JSFunction__JS_FUNCTION_TYPE");
  types_.kFixedArrayType = LoadConstant("type_FixedArray__FIXED_ARRAY_TYPE");
  types_.kJSArrayBufferType =
      LoadConstant("type_JSArrayBuffer__JS_ARRAY_BUFFER_TYPE");
  types_.kJSTypedArrayType =
      LoadConstant("type_JSTypedArray__JS_TYPED_ARRAY_TYPE");
}


int64_t LLV8::LoadConstant(const char* name) {
  return target_.FindFirstGlobalVariable((kConstantPrefix + name).c_str())
                .GetValueAsSigned(0);
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
      v8()->LoadValue<Value>(raw() + v8()->frame_.kContextOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_context = Smi(context);
  if (smi_context.Check() &&
      smi_context.GetValue() == v8()->frame_.kAdaptorFrame) {
    return "<adaptor>";
  }

  Value marker =
      v8()->LoadValue<Value>(raw() + v8()->frame_.kMarkerOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_marker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == v8()->frame_.kEntryFrame) {
      return "<entry>";
    } else if (value == v8()->frame_.kEntryConstructFrame) {
      return "<entry_construct>";
    } else if (value == v8()->frame_.kExitFrame) {
      return "<exit>";
    } else if (value == v8()->frame_.kInternalFrame) {
      return "<internal>";
    } else if (value == v8()->frame_.kConstructFrame) {
      return "<constructor>";
    } else if (value != v8()->frame_.kJSFrame &&
               value != v8()->frame_.kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker");
      return std::string();
    }
  }

  // We are dealing with function or internal code (probably stub)
  JSFunction fn =
      v8()->LoadValue<JSFunction>(raw() + v8()->frame_.kFunctionOffset, err);
  if (err.Fail()) return std::string();

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) return std::string();

  if (fn_type == v8()->types_.kCodeType) return "<internal code>";
  if (fn_type != v8()->types_.kJSFunctionType) return "<non-function>";

  std::string args;
  if (with_args) {
    args = InspectArgs(fn, err);
    if (err.Fail()) return std::string();
  }

  return fn.GetDebugLine(args, err);
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


std::string JSFunction::Inspect(Error& err) {
  return "<function: " + GetDebugLine(std::string(), err) + ">";
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
  if (type > v8()->types_.kFirstNonstringType) {
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
  Smi smi(this);
  if (smi.Check()) return false;

  HeapObject obj(this);
  if (!obj.Check()) return false;

  int64_t type = obj.GetType(err);
  if (err.Fail()) return false;

  if (type != v8()->types_.kOddballType) return false;

  Oddball odd(this);
  return odd.IsHoleOrUndefined(err);
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

  if (type < v8()->types_.kFirstNonstringType) {
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

  if (type == v8()->types_.kGlobalObjectType) return pre + "<Global>";
  if (type == v8()->types_.kCodeType) return pre + "<Code>";

  if (type == v8()->types_.kJSObjectType) {
    JSObject o(this);
    return pre + o.Inspect(detailed, err);
  }

  if (type == v8()->types_.kJSArrayType) {
    JSArray arr(this);
    return pre + arr.Inspect(detailed, err);
  }

  if (type == v8()->types_.kOddballType) {
    Oddball o(this);
    return pre + o.Inspect(err);
  }

  if (type == v8()->types_.kJSFunctionType) {
    JSFunction fn(this);
    return pre + fn.Inspect(err);
  }

  if (type < v8()->types_.kFirstNonstringType) {
    String str(this);
    return pre + str.Inspect(err);
  }

  if (type == v8()->types_.kFixedArrayType) {
    FixedArray arr(this);
    return pre + arr.Inspect(err);
  }

  if (type == v8()->types_.kJSArrayBufferType) {
    JSArrayBuffer buf(this);
    return pre + buf.Inspect(err);
  }

  if (type == v8()->types_.kJSTypedArrayType) {
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


std::string String::ToString(Error& err) {
  int64_t repr = Representation(err);
  if (err.Fail()) return std::string();

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (repr == v8()->string_.kSeqStringTag) {
    if (encoding == v8()->string_.kOneByteStringTag) {
      OneByteString one(this);
      return one.ToString(err);
    } else if (encoding == v8()->string_.kTwoByteStringTag) {
      TwoByteString two(this);
      return two.ToString(err);
    }

    err = Error::Failure("Unsupported seq string encoding");
    return std::string();
  }

  if (repr == v8()->string_.kConsStringTag) {
    ConsString cons(this);
    return cons.ToString(err);
  }

  if (repr == v8()->string_.kSlicedStringTag) {
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


std::string Oddball::Inspect(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return std::string();

  int64_t kind_val = kind.GetValue();
  if (kind_val == v8()->oddball_.kException) return "<exception>";
  if (kind_val == v8()->oddball_.kFalse) return "<false>";
  if (kind_val == v8()->oddball_.kTrue) return "<true>";
  if (kind_val == v8()->oddball_.kUndefined) return "<undefined>";
  if (kind_val == v8()->oddball_.kNull) return "<null>";
  if (kind_val == v8()->oddball_.kTheHole) return "<hole>";
  if (kind_val == v8()->oddball_.kUninitialized) return "<uninitialized>";
  return "<Oddball>";
}


std::string JSArrayBuffer::Inspect(Error& err) {
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
    if (type != v8()->types_.kMapType) break;
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

  if (constructor_type != v8()->types_.kJSFunctionType)
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
      res += " ";
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

    int64_t index = descriptors.FieldIndex(details);

    Value value;
    if (index < in_object_count)
      value = GetInObjectValue(instance_size, index - in_object_count, err);
    else
      value = extra_properties.Get<Value>(index - in_object_count, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    ." + key.ToString(err) + "=";
    if (err.Fail()) return std::string();

    res += value.Inspect(false, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


Value JSObject::GetInObjectValue(int64_t size, int index, Error& err) {
  return LoadFieldValue<Value>(size + index * v8()->kPointerSize, err);
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
