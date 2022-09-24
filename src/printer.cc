#include <cinttypes>
#include <iostream>
#include <sstream>

#include "deps/rang/include/rang.hpp"
#include "src/llv8-inl.h"
#include "src/printer.h"

namespace llnode {

// Forward declarations
template <>
std::string Printer::Stringify(v8::Context ctx, Error& err);

template <>
std::string Printer::Stringify(v8::Value value, Error& err);

template <>
std::string Printer::Stringify(v8::HeapObject heap_object, Error& err);

template <>
std::string Printer::Stringify(v8::JSFrame js_frame, Error& err) {
  v8::Value context = llv8_->LoadValue<v8::Value>(
      js_frame.raw() + llv8_->frame()->kContextOffset, err);
  if (err.Fail()) return std::string();

  v8::Smi smi_context = js_frame.FromFrameMarker(context);
  if (smi_context.Check() &&
      smi_context.GetValue() == llv8_->frame()->kAdaptorFrame) {
    return "<adaptor>";
  }

  v8::Value marker = llv8_->LoadValue<v8::Value>(
      js_frame.raw() + llv8_->frame()->kMarkerOffset, err);
  if (err.Fail()) return std::string();

  v8::Smi smi_marker = js_frame.FromFrameMarker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == llv8_->frame()->kEntryFrame) {
      return "<entry>";
    } else if (value == llv8_->frame()->kEntryConstructFrame) {
      return "<entry_construct>";
    } else if (value == llv8_->frame()->kExitFrame) {
      return "<exit>";
    } else if (value == llv8_->frame()->kInternalFrame) {
      return "<internal>";
    } else if (value == llv8_->frame()->kConstructFrame) {
      return "<constructor>";
    } else if (value == llv8_->frame()->kStubFrame) {
      return "<stub>";
    } else if (value != llv8_->frame()->kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker %" PRId64, value);
      return std::string();
    }
  }

  // We are dealing with function or internal code (probably stub)
  v8::JSFunction fn = js_frame.GetFunction(err);
  if (err.Fail()) return std::string();

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) return std::string();

  if (fn_type == llv8_->types()->kCodeType) return "<internal code>";
  if (fn_type != llv8_->types()->kJSFunctionType) return "<non-function>";

  std::string args;
  if (options_.with_args) {
    args = StringifyArgs(js_frame, fn, err);
    if (err.Fail()) return std::string();
  }

  char tmp[128];
  snprintf(tmp, sizeof(tmp), " fn=0x%016" PRIx64, fn.raw());
  return fn.GetDebugLine(args, err) + tmp;
}


template <>
std::string Printer::Stringify(v8::JSFunction js_function, Error& err) {
  std::string res =
      "<function: " + js_function.GetDebugLine(std::string(), err);

  if (err.Fail()) return std::string();

  if (options_.detailed) {
    v8::HeapObject context_obj = js_function.GetContext(err);
    if (err.Fail()) return std::string();

    v8::Context context(context_obj);

    std::stringstream ss;
    ss << rang::fg::magenta << res << rang::fg::reset << rang::style::bold
       << rang::fg::yellow << "\n  context" << rang::fg::reset
       << rang::style::reset << "=" << rang::fg::cyan << "0x" << std::hex
       << context_obj.raw() << std::dec << rang::fg::reset;
    res = ss.str();

    {
      PrinterOptions ctx_options;
      ctx_options.detailed = true;
      ctx_options.indent_depth = options_.indent_depth + 1;
      Printer printer(llv8_, ctx_options);
      std::string context_str = printer.Stringify(context, err);
      if (err.Fail()) return std::string();

      if (!context_str.empty()) res += ":" + context_str;
    }

    if (options_.print_source) {
      v8::SharedFunctionInfo info = js_function.Info(err);
      if (err.Fail()) return res;

      std::string name_str = info.ProperName(err);
      if (err.Fail()) return res;

      std::string source = js_function.GetSource(err);
      if (!err.Fail()) {
        res += "\n  source:\n";
        // name_str may be an empty string but that will match
        // the syntax for an anonymous function declaration correctly.
        res += "function " + name_str;
        res += source + "\n";
      }
    }
    res = res + ">";
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res << ">" << rang::fg::reset;
    res = ss.str();
  }
  return res;
}


template <>
std::string Printer::Stringify(v8::JSDate js_date, Error& err) {
  std::stringstream ss;
  ss << rang::fg::yellow << "<JSDate: " + js_date.ToString(err) + ">"
     << rang::fg::reset;
  return ss.str();
}

template <>
std::string Printer::Stringify(v8::Smi smi, Error& err) {
  std::stringstream ss;
  ss << rang::fg::yellow << "<Smi: " + smi.ToString(err) + ">"
     << rang::fg::reset;
  return ss.str();
}

template <>
std::string Printer::Stringify(v8::HeapNumber heap_number, Error& err) {
  std::stringstream ss;
  ss << rang::fg::yellow << "<Number: " + heap_number.ToString(true, err) + ">"
     << rang::fg::reset;
  return ss.str();
}

template <>
std::string Printer::Stringify(v8::String str, Error& err) {
  std::string val = str.ToString(err);
  if (err.Fail()) return std::string();

  unsigned int len = options_.length;

  if (len != 0 && val.length() > len) val = val.substr(0, len) + "...";

  std::stringstream ss;
  ss << rang::fg::yellow << "<String: \"" + val + "\">" << rang::fg::reset;
  return ss.str();
}


template <>
std::string Printer::Stringify(v8::FixedArray fixed_array, Error& err) {
  v8::Smi length_smi = fixed_array.Length(err);
  if (err.Fail()) return std::string();

  std::string res = "<FixedArray, len=" + length_smi.ToString(err);
  if (err.Fail()) return std::string();

  if (options_.detailed) {
    std::stringstream ss;
    ss << rang::fg::magenta << res;
    std::string contents =
        StringifyContents(fixed_array, length_smi.GetValue(), err);
    if (!contents.empty()) {
      ss << " contents" << rang::fg::reset << "={\n" + contents + "}";
      res = ss.str();
    }
    res += ">";
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res + ">" << rang::fg::reset;
    res = ss.str();
  }
  return res;
}


template <>
std::string Printer::Stringify(v8::Context ctx, Error& err) {
  std::string res = "<Context";

  if (!options_.detailed) {
    return res + ">";
  }

  res += ": {\n";

  v8::Value previous = ctx.Previous(err);
  if (err.Fail()) return std::string();

  v8::HeapObject scope_obj = ctx.GetScopeInfo(err);
  if (err.Fail()) return std::string();

  v8::ScopeInfo scope(scope_obj);

  v8::HeapObject heap_previous = v8::HeapObject(previous);
  if (heap_previous.Check()) {
    std::stringstream ss;
    ss << rang::style::bold << rang::fg::yellow << options_.get_indent_spaces()
       << "(previous)" << rang::fg::reset << rang::style::reset << "="
       << rang::fg::cyan << "0x" << std::hex << previous.raw() << std::dec
       << rang::fg::reset << ":" << rang::fg::yellow << "<Context>"
       << rang::fg::reset << ",";

    res += ss.str();
  }

  if (!res.empty()) res += "\n";

  if (llv8_->context()->hasClosure()) {
    v8::JSFunction closure = ctx.Closure(err);
    if (err.Fail()) return std::string();

    std::stringstream ss;
    ss << rang::style::bold << rang::fg::yellow << options_.get_indent_spaces()
       << "(closure)" << rang::fg::reset << rang::style::reset << "="
       << rang::fg::cyan << "0x" << std::hex << closure.raw() << std::dec
       << rang::fg::reset << " {";
    res += ss.str();

    Printer printer(llv8_);
    res += printer.Stringify(closure, err) + "}";
    if (err.Fail()) return std::string();
  } else {
    // Scope for string stream
    {
      std::stringstream ss;
      ss << rang::style::bold << rang::fg::yellow
         << options_.get_indent_spaces() << "(scope_info)" << rang::fg::reset
         << rang::style::reset << "=" << rang::fg::cyan << "0x" << std::hex
         << scope.raw() << std::dec << rang::fg::yellow;
      res += ss.str() + ":<ScopeInfo";
    }

    Error function_name_error;
    v8::HeapObject maybe_function_name =
        scope.MaybeFunctionName(function_name_error);

    if (function_name_error.Success()) {
      res += ": for function " + v8::String(maybe_function_name).ToString(err);
    }

    // Scope for string stream
    {
      std::stringstream ss;
      ss << rang::fg::reset;
      res += ">" + ss.str();
    }
  }

  v8::Context::Locals locals(&ctx, err);
  if (err.Fail()) return std::string();
  for (v8::Context::Locals::Iterator it = locals.begin(); it != locals.end();
       it++) {
    v8::String name = it.LocalName(err);
    if (err.Fail()) return std::string();
    if (!res.empty()) res += ",\n";

    std::stringstream ss;
    ss << options_.get_indent_spaces() << rang::style::bold << rang::fg::yellow
       << name.ToString(err) << rang::fg::reset << rang::style::reset;
    res += ss.str() + "=";
    if (err.Fail()) return std::string();

    v8::Value value = it.GetValue(err);
    if (err.Fail()) return std::string();

    Printer printer(llv8_);
    res += printer.Stringify(value, err);
    if (err.Fail()) return std::string();
  }

  return res + "}>";
}


template <>
std::string Printer::Stringify(v8::Oddball oddball, Error& err) {
  v8::Smi kind = oddball.Kind(err);
  if (err.Fail()) return std::string();

  int64_t kind_val = kind.GetValue();
  std::string str;
  if (kind_val == llv8_->oddball()->kException)
    str = "<exception>";
  else if (kind_val == llv8_->oddball()->kFalse)
    str = "<false>";
  else if (kind_val == llv8_->oddball()->kTrue)
    str = "<true>";
  else if (kind_val == llv8_->oddball()->kUndefined)
    str = "<undefined>";
  else if (kind_val == llv8_->oddball()->kNull)
    str = "<null>";
  else if (kind_val == llv8_->oddball()->kTheHole)
    str = "<hole>";
  else if (kind_val == llv8_->oddball()->kUninitialized)
    str = "<uninitialized>";
  else
    str = "<Oddball>";

  std::stringstream ss;
  ss << rang::fg::yellow << str << rang::fg::reset;
  return ss.str();
}

template <>
std::string Printer::Stringify(v8::JSArrayBuffer js_array_buffer, Error& err) {
  bool neutered = js_array_buffer.WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) {
    std::stringstream ss;
    ss << rang::fg::yellow << "<ArrayBuffer [neutered]>" << rang::fg::reset;
    return ss.str();
  }

  v8::CheckedType<uintptr_t> data = js_array_buffer.BackingStore();
  v8::CheckedType<size_t> byte_length = js_array_buffer.ByteLength();

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "<ArrayBuffer: backingStore=%s, byteLength=%s",
           data.ToString("0x%016" PRIx64).c_str(),
           byte_length.ToString("%d").c_str());

  std::string res;
  res += tmp;
  if (options_.detailed) {
    std::stringstream ss;
    ss << rang::fg::magenta << res + ":" << rang::fg::yellow;
    res = ss.str();

    if (!(data.Check() && byte_length.Check())) {
      res = "";
    } else {
      res += " [\n  ";

      size_t display_length = std::min<size_t>(*byte_length, options_.length);
      res += llv8_->LoadBytes(*data, display_length, err);

      if (display_length < *byte_length) {
        res += " ...";
      }

      res += "\n]";
    }

    ss.str("");
    ss.clear();
    ss << res << rang::fg::reset;

    res = ss.str();
    return res + ">";
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res + ">" << rang::fg::reset;
    return ss.str();
  }
}


template <>
std::string Printer::Stringify(v8::JSTypedArray js_typed_array, Error& err) {
  // TODO(mmarchini): shouldn't need to fetch buffer here
  v8::JSArrayBuffer buf = js_typed_array.Buffer(err);
  if (err.Fail()) return std::string();

  bool neutered = buf.WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) {
    std::stringstream ss;
    ss << rang::fg::yellow << "<ArrayBufferView [neutered]>" << rang::fg::reset;
    return ss.str().c_str();
  }

  v8::CheckedType<uintptr_t> data = js_typed_array.GetData();
  // TODO(mmarchini): be more lenient to failed load
  RETURN_IF_INVALID(data, std::string());

  v8::CheckedType<size_t> byte_offset = js_typed_array.ByteOffset();
  RETURN_IF_INVALID(byte_offset, std::string());

  v8::CheckedType<size_t> byte_length = js_typed_array.ByteLength();
  RETURN_IF_INVALID(byte_length, std::string());

  char tmp[128];
  snprintf(tmp, sizeof(tmp),
           "<ArrayBufferView: backingStore=%s, byteOffset=%s, byteLength=%s",
           data.ToString("0x%016" PRIx64).c_str(),
           byte_offset.ToString("%d").c_str(),
           byte_length.ToString("%d").c_str());

  std::string res;
  res += tmp;
  if (options_.detailed) {
    std::stringstream ss;
    ss << rang::fg::magenta << res + ":" << rang::fg::yellow;
    res = ss.str();

    res += " [\n  ";

    size_t display_length = std::min<size_t>(*byte_length, options_.length);
    res += llv8_->LoadBytes(*data + *byte_offset, display_length, err);

    if (display_length < *byte_length) {
      res += " ...";
    }

    res += "\n]";

    ss.str("");
    ss.clear();
    ss << res << rang::fg::reset;
    res = ss.str();

    return res + ">";
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res + ">" << rang::fg::reset;
    return ss.str();
  }
}


template <>
std::string Printer::Stringify(v8::Map map, Error& err) {
  // TODO(mmarchini): don't fail if can't load NumberOfOwnDescriptors
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return std::string();

  std::string in_object_properties_or_constructor;
  int64_t in_object_properties_or_constructor_index;
  if (map.IsJSObjectMap(err)) {
    if (err.Fail()) return std::string();
    in_object_properties_or_constructor_index = map.InObjectProperties(err);
    in_object_properties_or_constructor = std::string("in_object_size");
  } else {
    in_object_properties_or_constructor_index =
        map.ConstructorFunctionIndex(err);
    in_object_properties_or_constructor = std::string("constructor_index");
  }
  if (err.Fail()) return std::string();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return std::string();

  char tmp[256];
  std::stringstream ss;
  ss << rang::fg::yellow
     << "<Map own_descriptors=%d %s=%d instance_size=%d descriptors=";

  // TODO(mmarchini): this should be a reusable method
  std::string descriptors_str = "???";
  v8::HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (descriptors_obj.Check()) {
    char descriptors_raw[50];
    snprintf(descriptors_raw, 50, "0x%016" PRIx64, descriptors_obj.raw());
    ss << descriptors_raw;
  } else {
    PRINT_DEBUG("Failed to load InstanceDescriptors");
    ss << rang::fg::red << "???";
  }

  ss << rang::fg::reset;

  snprintf(tmp, sizeof(tmp), ss.str().c_str(),
           static_cast<int>(own_descriptors_count),
           in_object_properties_or_constructor.c_str(),
           static_cast<int>(in_object_properties_or_constructor_index),
           static_cast<int>(instance_size));

  if (!options_.detailed) {
    return std::string(tmp) + ">";
  }

  if (descriptors_obj.Check()) {
    v8::DescriptorArray descriptors(descriptors_obj);
    if (err.Fail()) return std::string();

    return std::string(tmp) + ":" +
           Stringify<v8::FixedArray>(descriptors, err) + ">";
  } else {
    return std::string(tmp) + ">";
  }
}

template <>
std::string Printer::Stringify(v8::JSError js_error, Error& err) {
  std::string name = js_error.GetName(err);
  if (err.Fail()) return std::string();

  std::stringstream output;

  output << rang::fg::yellow << "<Object: " << name;

  // Print properties in detailed mode
  if (options_.detailed) {
    output << StringifyJSObjectFields(js_error, err);

    if (js_error.HasStackTrace(err)) {
      v8::StackTrace stack_trace = js_error.GetStackTrace(err);

      std::stringstream error_stack;
      error_stack << std::endl
                  << rang::fg::red << "  error stack" << rang::fg::reset << " {"
                  << std::endl;


      Printer printer(llv8_);
      for (v8::StackFrame frame : stack_trace) {
        v8::JSFunction js_function = frame.GetFunction(err);
        if (err.Fail()) {
          error_stack << rang::fg::gray << "    <unknown>" << std::endl;
          continue;
        }

        error_stack << "    "
                    << printer.Stringify<v8::HeapObject>(js_function, err)
                    << std::endl;
      }

      error_stack << "  }";
      output << error_stack.str();
    }
  }

  output << rang::fg::yellow << ">" << rang::fg::reset;
  return output.str();
}


template <>
std::string Printer::Stringify(v8::JSObject js_object, Error& err) {
  std::string name = js_object.GetName(err);
  if (err.Fail()) return std::string();

  std::stringstream output;

  output << rang::fg::yellow << "<Object: " << name;

  // Print properties in detailed mode
  if (options_.detailed) output << StringifyJSObjectFields(js_object, err);

  output << rang::fg::yellow << ">" << rang::fg::reset;

  return output.str();
}

std::string Printer::StringifyJSObjectFields(v8::JSObject js_object,
                                             Error& err) {
  std::stringstream output;

  output << rang::fg::reset << " " << StringifyProperties(js_object, err);
  if (err.Fail()) return std::string();

  std::string fields = StringifyInternalFields(js_object, err);
  if (err.Fail()) return std::string();

  if (!fields.empty()) {
    output << std::endl
           << rang::fg::magenta << "  internal fields" << rang::fg::reset
           << " {" << std::endl
           << fields << "}";
  }

  return output.str();
}

template <>
std::string Printer::Stringify(v8::JSArray js_array, Error& err) {
  int64_t length = js_array.GetArrayLength(err);
  if (err.Fail()) return std::string();

  std::string res = "<Array: length=" + std::to_string(length);

  if (options_.detailed) {
    int64_t display_length = std::min<int64_t>(length, options_.length);
    std::string elems = StringifyElements(js_array, display_length, err);
    if (err.Fail()) return std::string();

    std::stringstream ss;
    ss << rang::fg::magenta << res << rang::fg::reset;
    res = ss.str().c_str();

    if (!elems.empty()) res += " {\n" + elems + "}>";
    return res;
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res + ">" << rang::fg::reset;
    res = ss.str().c_str();
    return res;
  }
}


template <>
std::string Printer::Stringify(v8::JSRegExp regexp, Error& err) {
  if (llv8_->js_regexp()->kSourceOffset == -1)
    return Stringify<v8::JSObject>(regexp, err);

  std::string res = "<JSRegExp ";

  v8::String src = regexp.GetSource(err);
  if (err.Fail()) return std::string();
  res += "source=/" + src.ToString(err) + "/";
  if (err.Fail()) return std::string();

  // Print properties in detailed mode
  if (options_.detailed) {
    std::stringstream ss;
    ss << rang::fg::magenta << res << rang::fg::reset;
    res = ss.str();

    res += " " + StringifyProperties(regexp, err);
    res += ">";
    if (err.Fail()) return std::string();
  } else {
    std::stringstream ss;
    ss << rang::fg::yellow << res + ">" << rang::fg::reset;
    res = ss.str();
  }
  return res;
}

template <>
std::string Printer::Stringify(v8::HeapObject heap_object, Error& err) {
  int64_t type = heap_object.GetType(err);
  if (err.Fail()) return std::string();

  // TODO(indutny): make this configurable
  char buf[64];
  if (options_.print_map) {
    v8::HeapObject map = heap_object.GetMap(err);
    if (err.Fail()) return std::string();

    snprintf(buf, sizeof(buf),
             "0x%016" PRIx64 "(map=0x%016" PRIx64 "):", heap_object.raw(),
             map.raw());
  } else {
    std::stringstream ss;
    ss << rang::fg::cyan << "0x" << std::hex << heap_object.raw() << std::dec
       << rang::fg::reset;
    snprintf(buf, sizeof(buf), "%s:", ss.str().c_str());
  }

  std::string pre = buf;

  if (type == llv8_->types()->kGlobalObjectType) {
    std::stringstream ss;
    ss << rang::fg::yellow << "<Global>" << rang::fg::reset;
    return pre + ss.str();
  }
  if (type == llv8_->types()->kGlobalProxyType) {
    std::stringstream ss;
    ss << rang::fg::yellow << "<Global proxy>" << rang::fg::reset;
    return pre + ss.str();
  }
  if (type == llv8_->types()->kCodeType) {
    std::stringstream ss;
    ss << rang::fg::yellow << "<Code>" << rang::fg::reset;
    return pre + ss.str();
  }
  if (type == llv8_->types()->kMapType) {
    v8::Map m(heap_object);
    return pre + Stringify(m, err);
  }

  if (heap_object.IsJSErrorType(err)) {
    v8::JSError error(heap_object);
    return pre + Stringify(error, err);
  }

  if (v8::JSObject::IsObjectType(llv8_, type)) {
    v8::JSObject o(heap_object);
    return pre + Stringify(o, err);
  }

  if (type == llv8_->types()->kHeapNumberType) {
    v8::HeapNumber n(heap_object);
    return pre + Stringify(n, err);
  }

  if (type == llv8_->types()->kJSArrayType) {
    v8::JSArray arr(heap_object);
    return pre + Stringify(arr, err);
  }

  if (type == llv8_->types()->kOddballType) {
    v8::Oddball o(heap_object);
    return pre + Stringify(o, err);
  }

  if (type == llv8_->types()->kJSFunctionType) {
    v8::JSFunction fn(heap_object);
    return pre + Stringify(fn, err);
  }

  if (type == *llv8_->types()->kJSRegExpType) {
    v8::JSRegExp re(heap_object);
    return pre + Stringify(re, err);
  }

  if (type < llv8_->types()->kFirstNonstringType) {
    v8::String str(heap_object);
    return pre + Stringify(str, err);
  }

  if (type >= llv8_->types()->kFirstContextType &&
      type <= llv8_->types()->kLastContextType) {
    v8::Context ctx(heap_object);
    return pre + Stringify(ctx, err);
  }

  if (type == llv8_->types()->kFixedArrayType) {
    v8::FixedArray arr(heap_object);
    return pre + Stringify(arr, err);
  }

  if (type == llv8_->types()->kJSArrayBufferType) {
    v8::JSArrayBuffer buf(heap_object);
    return pre + Stringify(buf, err);
  }

  if (type == llv8_->types()->kJSTypedArrayType) {
    v8::JSTypedArray typed_array(heap_object);
    return pre + Stringify(typed_array, err);
  }

  if (type == llv8_->types()->kJSDateType) {
    v8::JSDate date(heap_object);
    return pre + Stringify(date, err);
  }

  PRINT_DEBUG("Unknown HeapObject Type %" PRId64 " at 0x%016" PRIx64 "", type,
              heap_object.raw());

  std::stringstream ss;
  ss << rang::fg::yellow << "<unknown>" << rang::fg::reset;
  return pre + ss.str().c_str();
}

template <>
std::string Printer::Stringify(v8::Value value, Error& err) {
  v8::Smi smi(value);
  if (smi.Check()) return Stringify(smi, err);

  v8::HeapObject obj(value);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return Stringify(obj, err);
}

std::string Printer::StringifyInternalFields(v8::JSObject js_object,
                                             Error& err) {
  v8::HeapObject map_obj = js_object.GetMap(err);
  if (err.Fail()) return std::string();

  v8::Map map(map_obj);
  int64_t type = map.GetType(err);
  if (err.Fail()) return std::string();

  // Only v8::JSObject for now
  if (!v8::JSObject::IsObjectType(llv8_, type)) return std::string();

  int64_t instance_size = map.InstanceSize(err);

  // kVariableSizeSentinel == 0
  // TODO(indutny): post-mortem constant for this?
  if (err.Fail() || instance_size == 0) return std::string();

  int64_t in_object_props = map.InObjectProperties(err);
  if (err.Fail()) return std::string();

  // in-object properties are appended to the end of the v8::JSObject,
  // skip them.
  instance_size -= in_object_props * llv8_->common()->kPointerSize;

  std::string res;
  std::stringstream ss;
  for (int64_t off = llv8_->js_object()->kInternalFieldsOffset;
       off < instance_size; off += llv8_->common()->kPointerSize) {
    int64_t field = js_object.LoadField(off, err);
    if (err.Fail()) return std::string();

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "    0x%016" PRIx64, field);

    if (!res.empty()) res += ",\n  ";

    ss.str("");
    ss.clear();
    ss << rang::fg::cyan << tmp << rang::fg::reset;
    res += ss.str();
  }

  return res;
}


std::string Printer::StringifyProperties(v8::JSObject js_object, Error& err) {
  std::string res;

  std::string elems = StringifyElements(js_object, err);
  if (err.Fail()) return std::string();

  if (!elems.empty()) {
    std::stringstream ss;
    ss << rang::fg::magenta << "elements" << rang::fg::reset << " {"
       << std::endl
       << elems << "}";
    res = ss.str();
  }

  v8::HeapObject map_obj = js_object.GetMap(err);
  if (err.Fail()) return std::string();

  v8::Map map(map_obj);

  std::string props;
  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return std::string();

  if (is_dict)
    props = StringifyDictionary(js_object, err);
  else
    props = StringifyDescriptors(js_object, map, err);

  if (err.Fail()) return std::string();

  if (!props.empty()) {
    if (!res.empty()) res += "\n  ";
    std::stringstream ss;
    ss << rang::fg::magenta << "properties" << rang::fg::reset << " {"
       << std::endl
       << props << "}";
    res += ss.str().c_str();
  }

  return res;
}

std::string Printer::StringifyElements(v8::JSObject js_object, Error& err) {
  v8::HeapObject elements_obj = js_object.Elements(err);
  if (err.Fail()) return std::string();

  v8::FixedArray elements(elements_obj);

  v8::Smi length_smi = elements.Length(err);
  if (err.Fail()) return std::string();

  int64_t length = length_smi.GetValue();
  return StringifyElements(js_object, length, err);
}


std::string Printer::StringifyElements(v8::JSObject js_object, int64_t length,
                                       Error& err) {
  v8::HeapObject elements_obj = js_object.Elements(err);
  if (err.Fail()) return std::string();
  v8::FixedArray elements(elements_obj);

  Printer printer(llv8_);

  std::string res;
  for (int64_t i = 0; i < length; i++) {
    v8::Value value = elements.Get<v8::Value>(i, err);
    if (err.Fail()) return std::string();

    bool is_hole = value.IsHole(err);
    if (err.Fail()) return std::string();

    // Skip holes
    if (is_hole) continue;

    if (!res.empty()) res += ",\n";

    std::stringstream ss;
    ss << rang::style::bold << rang::fg::yellow << "    ["
       << static_cast<int>(i) << "]" << rang::fg::reset << rang::style::reset
       << "=";
    res += ss.str();

    res += printer.Stringify(value, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

std::string Printer::StringifyDictionary(v8::JSObject js_object, Error& err) {
  v8::HeapObject dictionary_obj = js_object.Properties(err);
  if (err.Fail()) return std::string();

  v8::NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return std::string();

  Printer printer(llv8_);

  std::string res;
  std::stringstream ss;

  for (int64_t i = 0; i < length; i++) {
    v8::Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return std::string();

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return std::string();
    if (is_hole) continue;

    v8::Value value = dictionary.GetValue(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    ss.str("");
    ss.clear();
    ss << rang::style::bold << rang::fg::yellow << "    ." + key.ToString(err)
       << rang::fg::reset << rang::style::reset;
    res += ss.str() + "=";
    if (err.Fail()) return std::string();

    res += printer.Stringify(value, err);
    if (err.Fail()) return std::string();
  }

  return res;
}


std::string Printer::StringifyDescriptors(v8::JSObject js_object, v8::Map map,
                                          Error& err) {
  v8::HeapObject descriptors_obj = map.InstanceDescriptors(err);
  RETURN_IF_INVALID(descriptors_obj, std::string());

  v8::DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return std::string();

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return std::string();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return std::string();

  v8::HeapObject extra_properties_obj = js_object.Properties(err);
  if (err.Fail()) return std::string();

  v8::FixedArray extra_properties(extra_properties_obj);

  Printer printer(llv8_);

  std::string res;
  std::stringstream ss;
  for (int64_t i = 0; i < own_descriptors_count; i++) {
    if (!res.empty()) res += ",\n";

    v8::Value key = descriptors.GetKey(i);

    ss.str("");
    ss.clear();
    ss << rang::style::bold << rang::fg::yellow << "    .";
    if (key.Check()) {
      ss << key.ToString(err);
    } else {
      PRINT_DEBUG("Failed to get key for index %ld", i);
      ss << "???";
    }
    ss << rang::fg::reset << rang::style::reset;

    res += ss.str() + "=";
    if (err.Fail()) return std::string();

    v8::Smi details = descriptors.GetDetails(i);
    if (!details.Check()) {
      PRINT_DEBUG("Failed to get details for index %ld", i);
      res += "???";
      continue;
    }

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      v8::Value value;

      value = descriptors.GetValue(i);
      RETURN_IF_INVALID(value, std::string());
      if (err.Fail()) return std::string();

      res += printer.Stringify(value, err);
      if (err.Fail()) return std::string();
      continue;
    }

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      v8::HeapNumber value = js_object.GetDoubleField(index, err);

      Error value_err;
      res += value.ToString(true, value_err);
    } else {
      v8::Value value;
      if (index < 0)
        value =
            js_object.GetInObjectValue<v8::Value>(instance_size, index, err);
      else
        value = extra_properties.Get<v8::Value>(index, err);

      if (err.Fail()) return std::string();

      res += printer.Stringify(value, err);
    }
    if (err.Fail()) return std::string();
  }

  return res;
}

std::string Printer::StringifyContents(v8::FixedArray fixed_array, int length,
                                       Error& err) {
  std::string res;
  Printer printer(llv8_);

  for (int i = 0; i < length; i++) {
    v8::Value value = fixed_array.Get<v8::Value>(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    std::stringstream ss;
    ss << rang::style::bold << rang::fg::yellow << "    [" << i << "]"
       << rang::fg::reset << rang::style::reset << "=";
    res += ss.str() + printer.Stringify(value, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

std::string Printer::StringifyArgs(v8::JSFrame js_frame, v8::JSFunction fn,
                                   Error& err) {
  v8::SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) return std::string();

  int64_t param_count = info.ParameterCount(err);
  if (err.Fail()) return std::string();

  v8::Value receiver = js_frame.GetReceiver(param_count, err);
  if (err.Fail()) return std::string();

  Printer printer(llv8_);

  std::string res = "this=" + printer.Stringify(receiver, err);
  if (err.Fail()) return std::string();

  for (int64_t i = 0; i < param_count; i++) {
    v8::Value param = js_frame.GetParam(i, param_count, err);
    if (err.Fail()) return std::string();

    res += ", " + printer.Stringify(param, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

};  // namespace llnode
