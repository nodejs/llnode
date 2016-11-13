#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cinttypes>
#include <fstream>
#include <vector>

#include <lldb/API/SBExpressionOptions.h>

#include "src/llnode.h"
#include "src/llscan.h"
#include "src/llv8-inl.h"
#include "src/llv8.h"

namespace llnode {

// Defined in llnode.cc
extern v8::LLV8 llv8;

LLScan llscan;

using namespace lldb;


bool FindObjectsCmd::DoExecute(SBDebugger d, char** cmd,
                               SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  /* Ensure we have a map of objects. */
  if (!llscan.ScanHeapForObjects(target, result)) {
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  /* Create a vector to hold the entries sorted by instance count
   * TODO(hhellyer) - Make sort type an option (by count, size or name)
   */
  std::vector<TypeRecord*> sorted_by_count;
  TypeRecordMap::iterator end = llscan.GetMapsToInstances().end();
  for (TypeRecordMap::iterator it = llscan.GetMapsToInstances().begin();
       it != end; ++it) {
    sorted_by_count.push_back(it->second);
  }

  std::sort(sorted_by_count.begin(), sorted_by_count.end(),
            TypeRecord::CompareInstanceCounts);

  uint64_t total_objects = 0;

  result.Printf(" Instances  Total Size Name\n");
  result.Printf(" ---------- ---------- ----\n");

  for (std::vector<TypeRecord*>::iterator it = sorted_by_count.begin();
       it != sorted_by_count.end(); ++it) {
    TypeRecord* t = *it;
    result.Printf(" %10" PRId64 " %10" PRId64 " %s\n", t->GetInstanceCount(),
                  t->GetTotalInstanceSize(), t->GetTypeName().c_str());
    total_objects += t->GetInstanceCount();
  }

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


bool FindInstancesCmd::DoExecute(SBDebugger d, char** cmd,
                                 SBCommandReturnObject& result) {
  if (*cmd == NULL) {
    result.SetError("USAGE: v8 findjsinstances [-Fm] instance_name\n");
    return false;
  }

  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  /* Ensure we have a map of objects. */
  if (!llscan.ScanHeapForObjects(target, result)) {
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  v8::Value::InspectOptions inspect_options;

  inspect_options.detailed = detailed_;

  char** start = ParseInspectOptions(cmd, &inspect_options);

  std::string full_cmd;
  for (; start != nullptr && *start != nullptr; start++) full_cmd += *start;

  std::string type_name = full_cmd;

  // Load V8 constants from postmortem data
  llv8.Load(target);

  TypeRecordMap::iterator instance_it =
      llscan.GetMapsToInstances().find(type_name);
  if (instance_it != llscan.GetMapsToInstances().end()) {
    TypeRecord* t = instance_it->second;
    for (std::set<uint64_t>::iterator it = t->GetInstances().begin();
         it != t->GetInstances().end(); ++it) {
      v8::Error err;
      v8::Value v8_value(&llv8, *it);
      std::string res = v8_value.Inspect(&inspect_options, err);
      result.Printf("%s\n", res.c_str());
    }

  } else {
    result.Printf("No objects found with type name %s\n", type_name.c_str());
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


bool NodeInfoCmd::DoExecute(SBDebugger d, char** cmd,
                            SBCommandReturnObject& result) {
  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  /* Ensure we have a map of objects. */
  if (!llscan.ScanHeapForObjects(target, result)) {
    return false;
  }

  std::string process_type_name("process");

  TypeRecordMap::iterator instance_it =
      llscan.GetMapsToInstances().find(process_type_name);

  if (instance_it != llscan.GetMapsToInstances().end()) {
    TypeRecord* t = instance_it->second;
    for (std::set<uint64_t>::iterator it = t->GetInstances().begin();
         it != t->GetInstances().end(); ++it) {
      v8::Error err;

      // The properties object should be a JSObject
      v8::JSObject process_obj(&llv8, *it);


      v8::Value pid_val = process_obj.GetProperty("pid", err);

      if (pid_val.v8() != nullptr) {
        v8::Smi pid_smi(pid_val);
        result.Printf("Information for process id %" PRId64
                      " (process=0x%" PRIx64 ")\n",
                      pid_smi.GetValue(), process_obj.raw());
      } else {
        // This isn't the process object we are looking for.
        continue;
      }

      v8::Value platform_val = process_obj.GetProperty("platform", err);

      if (platform_val.v8() != nullptr) {
        v8::String platform_str(platform_val);
        result.Printf("Platform = %s, ", platform_str.ToString(err).c_str());
      }

      v8::Value arch_val = process_obj.GetProperty("arch", err);

      if (arch_val.v8() != nullptr) {
        v8::String arch_str(arch_val);
        result.Printf("Architecture = %s, ", arch_str.ToString(err).c_str());
      }

      v8::Value ver_val = process_obj.GetProperty("version", err);

      if (ver_val.v8() != nullptr) {
        v8::String ver_str(ver_val);
        result.Printf("Node Version = %s\n", ver_str.ToString(err).c_str());
      }

      // Note the extra s on versions!
      v8::Value versions_val = process_obj.GetProperty("versions", err);
      if (versions_val.v8() != nullptr) {
        v8::JSObject versions_obj(versions_val);

        std::vector<std::string> version_keys;

        // Get the list of keys on an object as strings.
        versions_obj.Keys(version_keys, err);

        std::sort(version_keys.begin(), version_keys.end());

        result.Printf("Component versions (process.versions=0x%" PRIx64 "):\n",
                      versions_val.raw());

        for (std::vector<std::string>::iterator key = version_keys.begin();
             key != version_keys.end(); ++key) {
          v8::Value ver_val = versions_obj.GetProperty(*key, err);
          if (ver_val.v8() != nullptr) {
            v8::String ver_str(ver_val);
            result.Printf("    %s = %s\n", key->c_str(),
                          ver_str.ToString(err).c_str());
          }
        }
      }

      v8::Value release_val = process_obj.GetProperty("release", err);
      if (release_val.v8() != nullptr) {
        v8::JSObject release_obj(release_val);

        std::vector<std::string> release_keys;

        // Get the list of keys on an object as strings.
        release_obj.Keys(release_keys, err);

        result.Printf("Release Info (process.release=0x%" PRIx64 "):\n",
                      release_val.raw());

        for (std::vector<std::string>::iterator key = release_keys.begin();
             key != release_keys.end(); ++key) {
          v8::Value ver_val = release_obj.GetProperty(*key, err);
          if (ver_val.v8() != nullptr) {
            v8::String ver_str(ver_val);
            result.Printf("    %s = %s\n", key->c_str(),
                          ver_str.ToString(err).c_str());
          }
        }
      }

      v8::Value execPath_val = process_obj.GetProperty("execPath", err);

      if (execPath_val.v8() != nullptr) {
        v8::String execPath_str(execPath_val);
        result.Printf("Executable Path = %s\n",
                      execPath_str.ToString(err).c_str());
      }

      v8::Value argv_val = process_obj.GetProperty("argv", err);

      if (argv_val.v8() != nullptr) {
        v8::JSArray argv_arr(argv_val);
        result.Printf("Command line arguments (process.argv=0x%" PRIx64 "):\n",
                      argv_val.raw());
        // argv is an array, which we can treat as a subtype of object.
        int64_t length = argv_arr.GetArrayLength(err);
        for (int64_t i = 0; i < length; ++i) {
          v8::Value element_val = argv_arr.GetArrayElement(i, err);
          if (element_val.v8() != nullptr) {
            v8::String element_str(element_val);
            result.Printf("    [%" PRId64 "] = '%s'\n", i,
                          element_str.ToString(err).c_str());
          }
        }
      }

      /* The docs for process.execArgv say "These options are useful in order
       * to spawn child processes with the same execution environment
       * as the parent." so being able to check these have been passed in
       * seems like a good idea.
       */
      v8::Value execArgv_val = process_obj.GetProperty("execArgv", err);

      if (argv_val.v8() != nullptr) {
        // Should possibly just treat this as an object in case anyone has
        // attached a property.
        v8::JSArray execArgv_arr(execArgv_val);
        result.Printf(
            "Node.js Comamnd line arguments (process.execArgv=0x%" PRIx64
            "):\n",
            execArgv_val.raw());
        // execArgv is an array, which we can treat as a subtype of object.
        int64_t length = execArgv_arr.GetArrayLength(err);
        for (int64_t i = 0; i < length; ++i) {
          v8::Value element_val = execArgv_arr.GetArrayElement(i, err);
          if (element_val.v8() != nullptr) {
            v8::String element_str(element_val);
            result.Printf("    [%" PRId64 "] = '%s'\n", i,
                          element_str.ToString(err).c_str());
          }
        }
      }
    }

  } else {
    result.Printf("No process objects found.\n");
  }

  return true;
}


bool FindReferencesCmd::DoExecute(SBDebugger d, char** cmd,
                                  SBCommandReturnObject& result) {
  if (*cmd == NULL) {
    result.SetError("USAGE: v8 findrefs expr\n");
    return false;
  }

  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  // Default scan type.
  ScanType type = ScanType::kFieldValue;

  char** start = ParseScanOptions(cmd, &type);

  if (*start == nullptr) {
    result.SetError("Missing search parameter");
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  // Load V8 constants from postmortem data
  llv8.Load(target);

  ObjectScanner* scanner;

  switch (type) {
    case ScanType::kFieldValue: {
      std::string full_cmd;
      for (; start != nullptr && *start != nullptr; start++) full_cmd += *start;

      SBExpressionOptions options;
      SBValue value = target.EvaluateExpression(full_cmd.c_str(), options);
      if (value.GetError().Fail()) {
        SBStream desc;
        if (value.GetError().GetDescription(desc)) {
          result.SetError(desc.GetData());
        }
        result.SetStatus(eReturnStatusFailed);
        return false;
      }
      // Check the address we've been given at least looks like a valid object.
      v8::Value search_value(&llv8, value.GetValueAsSigned());
      v8::Smi smi(search_value);
      if (smi.Check()) {
        result.SetError("Search value is an SMI.");
        result.SetStatus(eReturnStatusFailed);
        return false;
      }
      scanner = new ReferenceScanner(search_value);
      break;
    }
    case ScanType::kPropertyName: {
      // Check for extra parameters or parameters that needed quoting.
      if (start[1] != nullptr) {
        result.SetError("Extra search parameter or unquoted string specified.");
        result.SetStatus(eReturnStatusFailed);
        return false;
      }
      std::string property_name = start[0];
      scanner = new PropertyScanner(property_name);
      break;
    }
    case ScanType::kStringValue: {
      // Check for extra parameters or parameters that needed quoting.
      if (start[1] != nullptr) {
        result.SetError("Extra search parameter or unquoted string specified.");
        result.SetStatus(eReturnStatusFailed);
        return false;
      }
      std::string string_value = start[0];
      scanner = new StringScanner(string_value);
      break;
    }
    /* We can add options to the command and further sub-classes of
     * object scanner to do other searches, e.g.:
     * - Objects that refer to a particular string literal.
     *   (lldb) findreferences -s "Hello World!"
     */
    case ScanType::kBadOption: {
      result.SetError("Invalid search type");
      result.SetStatus(eReturnStatusFailed);
      return false;
    }
  }

  /* Ensure we have a map of objects.
   * (Do this after we've checked the options to avoid
   * a long pause before reporting an error.)
   */
  if (!llscan.ScanHeapForObjects(target, result)) {
    delete scanner;
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  // Walk all the object instances and handle them according to their type.
  TypeRecordMap mapstoinstances = llscan.GetMapsToInstances();
  for (auto const entry : mapstoinstances) {
    TypeRecord* typerecord = entry.second;
    for (uint64_t addr : typerecord->GetInstances()) {
      v8::Error err;
      v8::Value obj_value(&llv8, addr);
      v8::HeapObject heap_object(obj_value);
      int64_t type = heap_object.GetType(err);
      v8::LLV8* v8 = heap_object.v8();

      // We only need to handle the types that are in
      // FindJSObjectsVisitor::IsAHistogramType
      // as those are the only objects that end up in GetMapsToInstances
      if (type == v8->types()->kJSObjectType ||
          type == v8->types()->kJSArrayType) {
        // Objects can have elements and arrays can have named properties.
        // Basically we need to access objects and arrays as both objects and
        // arrays.
        v8::JSObject js_obj(heap_object);
        scanner->PrintRefs(result, js_obj, err);

      } else if (type < v8->types()->kFirstNonstringType) {
        v8::String str(heap_object);
        scanner->PrintRefs(result, str, err);

      } else if (type == v8->types()->kJSTypedArrayType) {
        // These should only point to off heap memory,
        // this case should be a no-op.
      } else {
        // result.Printf("Unhandled type: %" PRId64 " for addr %" PRIx64
        //    "\n", type, addr);
      }
    }
  }

  delete scanner;

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


char** FindReferencesCmd::ParseScanOptions(char** cmd, ScanType* type) {
  static struct option opts[] = {{"value", no_argument, nullptr, 'v'},
                                 {"name", no_argument, nullptr, 'n'},
                                 {"string", no_argument, nullptr, 's'},
                                 {nullptr, 0, nullptr, 0}};

  int argc = 1;
  for (char** p = cmd; p != nullptr && *p != nullptr; p++) argc++;

  char* args[argc];

  // Make this look like a command line, we need a valid element at index 0
  // for getopt_long to use in its error messages.
  char name[] = "llscan";
  args[0] = name;
  for (int i = 0; i < argc - 1; i++) args[i + 1] = cmd[i];

  bool found_scan_type = false;

  // Reset getopts.
  optind = 1;
  opterr = 1;
  do {
    int arg = getopt_long(argc, args, "vns", opts, nullptr);
    if (arg == -1) break;

    if (found_scan_type) {
      *type = ScanType::kBadOption;
      break;
    }

    switch (arg) {
      case 'v':
        *type = ScanType::kFieldValue;
        found_scan_type = true;
        break;
      case 'n':
        *type = ScanType::kPropertyName;
        found_scan_type = true;
        break;
      case 's':
        *type = ScanType::kStringValue;
        found_scan_type = true;
        break;
      default:
        *type = ScanType::kBadOption;
        break;
    }
  } while (true);

  return cmd + optind - 1;
}


void FindReferencesCmd::ReferenceScanner::PrintRefs(
    SBCommandReturnObject& result, v8::JSObject& js_obj, v8::Error& err) {
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;
  int64_t length = js_obj.GetArrayLength(err);
  for (int64_t i = 0; i < length; ++i) {
    v8::Value v = js_obj.GetArrayElement(i, err);

    // Array is borked, or not array at all - skip it
    if (!err.Success()) break;

    if (v.raw() != search_value_.raw()) continue;

    std::string type_name = js_obj.GetTypeName(&inspect_options, err);
    result.Printf("0x%" PRIx64 ": %s[%" PRId64 "]=0x%" PRIx64 "\n",
                  js_obj.raw(), type_name.c_str(), i, search_value_.raw());
  }

  // Walk all the properties in this object.
  // We only create strings for the field names that match the search
  // value.
  std::vector<std::pair<v8::Value, v8::Value>> entries = js_obj.Entries(err);
  if (err.Fail()) {
    return;
  }
  for (auto entry : entries) {
    v8::Value v = entry.second;
    if (v.raw() == search_value_.raw()) {
      std::string key = entry.first.ToString(err);
      std::string type_name = js_obj.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", js_obj.raw(),
                    type_name.c_str(), key.c_str(), search_value_.raw());
    }
  }
}


void FindReferencesCmd::ReferenceScanner::PrintRefs(
    SBCommandReturnObject& result, v8::String& str, v8::Error& err) {
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;
  v8::LLV8* v8 = str.v8();

  int64_t repr = str.Representation(err);

  // Concatenated and sliced strings refer to other strings so
  // we need to check their references.

  if (repr == v8->string()->kSlicedStringTag) {
    v8::SlicedString sliced_str(str);
    v8::String parent = sliced_str.Parent(err);
    if (err.Success() && parent.raw() == search_value_.raw()) {
      std::string type_name = sliced_str.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<Parent>", search_value_.raw());
    }
  } else if (repr == v8->string()->kConsStringTag) {
    v8::ConsString cons_str(str);

    v8::String first = cons_str.First(err);
    if (err.Success() && first.raw() == search_value_.raw()) {
      std::string type_name = cons_str.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<First>", search_value_.raw());
    }

    v8::String second = cons_str.Second(err);
    if (err.Success() && second.raw() == search_value_.raw()) {
      std::string type_name = cons_str.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<Second>", search_value_.raw());
    }
  }
  // Nothing to do for other kinds of string.
}


void FindReferencesCmd::PropertyScanner::PrintRefs(
    SBCommandReturnObject& result, v8::JSObject& js_obj, v8::Error& err) {
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;

  // (Note: We skip array elements as they don't have names.)

  // Walk all the properties in this object.
  // We only create strings for the field names that match the search
  // value.
  std::vector<std::pair<v8::Value, v8::Value>> entries = js_obj.Entries(err);
  if (err.Fail()) {
    return;
  }
  for (auto entry : entries) {
    v8::HeapObject nameObj(entry.first);
    std::string key = entry.first.ToString(err);
    if (err.Fail()) {
      continue;
    }
    if (key == search_value_) {
      std::string type_name = js_obj.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", js_obj.raw(),
                    type_name.c_str(), key.c_str(), entry.second.raw());
    }
  }
}


void FindReferencesCmd::StringScanner::PrintRefs(SBCommandReturnObject& result,
                                                 v8::JSObject& js_obj,
                                                 v8::Error& err) {
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;
  v8::LLV8* v8 = js_obj.v8();

  int64_t length = js_obj.GetArrayLength(err);
  for (int64_t i = 0; i < length; ++i) {
    v8::Value v = js_obj.GetArrayElement(i, err);
    if (err.Fail()) {
      continue;
    }
    v8::HeapObject valueObj(v);

    int64_t type = valueObj.GetType(err);
    if (err.Fail()) {
      continue;
    }
    if (type < v8->types()->kFirstNonstringType) {
      v8::String valueString(valueObj);
      std::string value = valueString.ToString(err);
      if (err.Fail()) {
        continue;
      }
      if (err.Success() && search_value_ == value) {
        std::string type_name = js_obj.GetTypeName(&inspect_options, err);

        result.Printf("0x%" PRIx64 ": %s[%" PRId64 "]=0x%" PRIx64 " '%s'\n",
                      js_obj.raw(), type_name.c_str(), i, v.raw(),
                      value.c_str());
      }
    }
  }

  // Walk all the properties in this object.
  // We only create strings for the field names that match the search
  // value.
  std::vector<std::pair<v8::Value, v8::Value>> entries = js_obj.Entries(err);
  if (err.Success()) {
    for (auto entry : entries) {
      v8::HeapObject valueObj(entry.second);
      int64_t type = valueObj.GetType(err);
      if (err.Fail()) {
        continue;
      }
      if (type < v8->types()->kFirstNonstringType) {
        v8::String valueString(valueObj);
        std::string value = valueString.ToString(err);
        if (err.Fail()) {
          continue;
        }
        if (search_value_ == value) {
          std::string key = entry.first.ToString(err);
          if (err.Fail()) {
            continue;
          }
          std::string type_name = js_obj.GetTypeName(&inspect_options, err);
          result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 " '%s'\n",
                        js_obj.raw(), type_name.c_str(), key.c_str(),
                        entry.second.raw(), value.c_str());
        }
      }
    }
  }
}


void FindReferencesCmd::StringScanner::PrintRefs(SBCommandReturnObject& result,
                                                 v8::String& str,
                                                 v8::Error& err) {
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;
  v8::LLV8* v8 = str.v8();

  // Concatenated and sliced strings refer to other strings so
  // we need to check their references.

  int64_t repr = str.Representation(err);
  if (err.Fail()) return;

  if (repr == v8->string()->kSlicedStringTag) {
    v8::SlicedString sliced_str(str);
    v8::String parent_str = sliced_str.Parent(err);
    if (err.Fail()) return;
    std::string parent = parent_str.ToString(err);
    if (err.Success() && search_value_ == parent) {
      std::string type_name = sliced_str.GetTypeName(&inspect_options, err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 " '%s'\n", str.raw(),
                    type_name.c_str(), "<Parent>", parent_str.raw(),
                    parent.c_str());
    }
  } else if (repr == v8->string()->kConsStringTag) {
    v8::ConsString cons_str(str);

    v8::String first_str = cons_str.First(err);
    if (err.Fail()) return;

    // It looks like sometimes one of the strings can be <null> or another
    // value,
    // verify that they are a JavaScript String before calling ToString.
    int64_t first_type = first_str.GetType(err);
    if (err.Fail()) return;

    if (first_type < v8->types()->kFirstNonstringType) {
      std::string first = first_str.ToString(err);

      if (err.Success() && search_value_ == first) {
        std::string type_name = cons_str.GetTypeName(&inspect_options, err);
        result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 " '%s'\n", str.raw(),
                      type_name.c_str(), "<First>", first_str.raw(),
                      first.c_str());
      }
    }

    v8::String second_str = cons_str.Second(err);
    if (err.Fail()) return;

    // It looks like sometimes one of the strings can be <null> or another
    // value,
    // verify that they are a JavaScript String before calling ToString.
    int64_t second_type = second_str.GetType(err);
    if (err.Fail()) return;

    if (second_type < v8->types()->kFirstNonstringType) {
      std::string second = second_str.ToString(err);

      if (err.Success() && search_value_ == second) {
        std::string type_name = cons_str.GetTypeName(&inspect_options, err);
        result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 " '%s'\n", str.raw(),
                      type_name.c_str(), "<Second>", second_str.raw(),
                      second.c_str());
      }
    }
  }
  // Nothing to do for other kinds of string.
  // They are strings so we will find references to them.
}


FindJSObjectsVisitor::FindJSObjectsVisitor(SBTarget& target,
                                           TypeRecordMap& mapstoinstances)
    : target_(target), mapstoinstances_(mapstoinstances) {
  found_count_ = 0;
  address_byte_size_ = target_.GetProcess().GetAddressByteSize();
  // Load V8 constants from postmortem data
  llv8.Load(target);
}


/* Visit every address, a bit brute force but it works. */
uint64_t FindJSObjectsVisitor::Visit(uint64_t location, uint64_t word) {
  v8::Value v8_value(&llv8, word);

  v8::Error err;
  // Test if this is SMI
  // Skip inspecting things that look like Smi's, they aren't objects.
  v8::Smi smi(v8_value);
  if (smi.Check()) return address_byte_size_;

  v8::HeapObject heap_object(v8_value);
  if (!heap_object.Check()) return address_byte_size_;

  v8::HeapObject map_object = heap_object.GetMap(err);
  if (err.Fail() || !map_object.Check()) return address_byte_size_;

  v8::Map map(map_object);

  MapCacheEntry map_info;
  if (map_cache_.count(map.raw()) == 0) {
    v8::Value::InspectOptions inspect_options;
    inspect_options.detailed = false;
    inspect_options.print_map = false;
    inspect_options.print_source = false;
    inspect_options.string_length = 0;

    // Check type first
    map_info.is_histogram = IsAHistogramType(map, err);

    // On success load type name
    if (map_info.is_histogram)
      map_info.type_name = heap_object.GetTypeName(&inspect_options, err);

    // Cache result
    map_cache_.insert(std::pair<int64_t, MapCacheEntry>(map.raw(), map_info));

    if (err.Fail()) return address_byte_size_;
  } else {
    map_info = map_cache_.at(map.raw());
  }

  if (!map_info.is_histogram) return address_byte_size_;

  /* No entry in the map, create a new one. */
  if (mapstoinstances_.count(map_info.type_name) == 0) {
    TypeRecord* t = new TypeRecord(map_info.type_name);

    t->AddInstance(word, map.InstanceSize(err));
    mapstoinstances_.insert(
        std::pair<std::string, TypeRecord*>(map_info.type_name, t));

  } else {
    /* Update an existing instance, if we haven't seen this instance before. */
    TypeRecord* t = mapstoinstances_.at(map_info.type_name);
    /* Determine if this is a new instance.
     * (We are scanning pointers to objects, we may have seen this location
     * before.)
     */
    if (t->GetInstances().count(word) == 0) {
      t->AddInstance(word, map.InstanceSize(err));
    }
  }

  if (err.Fail()) {
    return address_byte_size_;
  }

  found_count_++;

  /* Just advance one word.
   * (Should advance by object size, assuming objects can't overlap!)
   */
  return address_byte_size_;
}


bool FindJSObjectsVisitor::IsAHistogramType(v8::Map& map, v8::Error& err) {
  int64_t type = map.GetType(err);
  if (err.Fail()) return false;

  v8::LLV8* v8 = map.v8();
  if (type == v8->types()->kJSObjectType) return true;
  if (type == v8->types()->kJSArrayType) return true;
  if (type == v8->types()->kJSTypedArrayType) return true;
  if (type < v8->types()->kFirstNonstringType) return true;
  return false;
}


bool LLScan::ScanHeapForObjects(lldb::SBTarget target,
                                lldb::SBCommandReturnObject& result) {
  /* Check the last scan is still valid - the process hasn't moved
   * and we haven't changed target.
   */

  // Reload process anyway
  process_ = target.GetProcess();

  // Need to reload memory ranges (though this does assume the user has also
  // updated
  // LLNODE_RANGESFILE with data for the new dump or things won't match up).
  if (target_ != target) {
    ClearMemoryRanges();
    mapstoinstances_.clear();
    target_ = target;
  }

#ifndef LLDB_SBMemoryRegionInfoList_h_
  /* Fall back to environment variable containing pre-parsed list of memory
   * ranges. */
  if (nullptr == ranges_) {
    const char* segmentsfilename = getenv("LLNODE_RANGESFILE");

    if (segmentsfilename == nullptr) {
      result.SetError(
          "No memory range information available for this process. Cannot scan "
          "for objects.\n"
          "Please set `LLNODE_RANGESFILE` environment variable\n");
      return false;
    }

    if (!GenerateMemoryRanges(target, segmentsfilename)) {
      result.SetError(
          "No memory range information available for this process. Cannot scan "
          "for objects.\n");
      return false;
    }
  }
#endif  // LLDB_SBMemoryRegionInfoList_h_

  /* If we've reached here we have access to information about the valid memory
   * ranges in the process and can scan for objects.
   */

  /* Populate the map of objects. */
  if (mapstoinstances_.empty()) {
    FindJSObjectsVisitor v(target, GetMapsToInstances());

    ScanMemoryRanges(v);
  }

  return true;
}


void LLScan::ScanMemoryRanges(FindJSObjectsVisitor& v) {
  bool done = false;

  const uint64_t addr_size = process_.GetAddressByteSize();

  // Pages are usually around 1mb, so this should more than enough
  const uint64_t block_size = 1024 * 1024 * addr_size;
  unsigned char* block = new unsigned char[block_size];

#ifndef LLDB_SBMemoryRegionInfoList_h_
  MemoryRange* head = ranges_;

  while (head != nullptr && !done) {
    uint64_t address = head->start_;
    uint64_t len = head->length_;
    head = head->next_;

#else  // LLDB_SBMemoryRegionInfoList_h_
  SBMemoryRegionInfoList memory_regions = process_.GetMemoryRegions();
  SBMemoryRegionInfo region_info;

  for (uint32_t i = 0; i < memory_regions.GetSize(); ++i) {
    memory_regions.GetMemoryRegionAtIndex(i, region_info);

    if (!region_info.IsWritable()) {
      continue;
    }

    uint64_t address = region_info.GetRegionBase();
    uint64_t len = region_info.GetRegionEnd() - region_info.GetRegionBase();

#endif  // LLDB_SBMemoryRegionInfoList_h_
    /* Brute force search - query every address - but allow the visitor code to
     * say how far to move on so we don't read every byte.
     */

    SBError sberr;
    uint64_t address_end = address + len;

    // Load data in blocks to speed up whole process
    for (auto searchAddress = address; searchAddress < address_end;
         searchAddress += block_size) {
      size_t loaded = std::min(address_end - searchAddress, block_size);
      process_.ReadMemory(searchAddress, block, loaded, sberr);
      if (sberr.Fail()) {
        // TODO(indutny): add error information
        break;
      }

      uint32_t increment = 1;
      for (size_t j = 0; j + addr_size <= loaded;) {
        uint64_t value;

        if (addr_size == 4)
          value = *reinterpret_cast<uint32_t*>(&block[j]);
        else if (addr_size == 8)
          value = *reinterpret_cast<uint64_t*>(&block[j]);
        else
          break;

        increment = v.Visit(j + searchAddress, value);
        if (increment == 0) break;

        j += static_cast<size_t>(increment);
      }

      if (increment == 0) {
        done = true;
        break;
      }
    }
  }

  delete[] block;
}


/* Read a file of memory ranges parsed from the core dump.
 * This is a work around for the lack of an API to get the memory ranges
 * within lldb.
 * There are scripts for generating this file on Mac and Linux stored in
 * the scripts directory of the llnode repository.
 * Export the name or full path to the ranges file in the LLNODE_RANGESFILE
 * env var before starting lldb and loading the llnode plugin.
 */
bool LLScan::GenerateMemoryRanges(lldb::SBTarget target,
                                  const char* segmentsfilename) {
  std::ifstream input(segmentsfilename);

  if (!input.is_open()) {
    return false;
  }

  uint64_t address = 0;
  uint64_t len = 0;

  MemoryRange** tailptr = &ranges_;

  lldb::addr_t address_byte_size = target.GetProcess().GetAddressByteSize();

  while (input >> std::hex >> address >> std::hex >> len) {
    /* Check if the range is accessible.
     * The structure of a core file means if you check the start and the end of
     * a range then the middle will be there, ranges are contiguous in the file,
     * but cores often get truncated due to file size limits so ranges can be
     * missing or truncated. Sometimes shared memory segments are omitted so
     * it's also possible an entire section could be missing from the middle.
     */
    lldb::SBError error;

    target.GetProcess().ReadPointerFromMemory(address, error);
    if (!error.Success()) {
      /* Could not access first word, skip. */
      continue;
    }

    target.GetProcess().ReadPointerFromMemory(
        (address + len) - address_byte_size, error);
    if (!error.Success()) {
      /* Could not access last word, skip. */
      continue;
    }

    MemoryRange* newRange = new MemoryRange(address, len);

    *tailptr = newRange;
    tailptr = &(newRange->next_);
  }
  return true;
}


void LLScan::ClearMemoryRanges() {
  MemoryRange* head = ranges_;
  while (head != nullptr) {
    MemoryRange* range = head;
    head = head->next_;
    delete range;
  }
  ranges_ = nullptr;
}


void LLScan::ClearMapsToInstances() {
  TypeRecordMap::iterator end = GetMapsToInstances().end();
  for (TypeRecordMap::iterator it = GetMapsToInstances().begin(); it != end;
       ++it) {
    TypeRecord* t = it->second;
    delete t;
  }
  GetMapsToInstances().clear();
}
}
