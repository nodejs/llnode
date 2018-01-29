#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cinttypes>
#include <fstream>
#include <vector>
#include <iostream>

#include <lldb/API/SBExpressionOptions.h>

#include "src/llnode.h"
#include "src/llscan.h"
#include "src/llv8-inl.h"
#include "src/llv8.h"

namespace llnode {

using lldb::ByteOrder;
using lldb::SBCommandReturnObject;
using lldb::SBDebugger;
using lldb::SBError;
using lldb::SBExpressionOptions;
using lldb::SBStream;
using lldb::SBTarget;
using lldb::SBValue;
using lldb::eReturnStatusFailed;
using lldb::eReturnStatusSuccessFinishResult;

// Defined in llnode.cc
extern v8::LLV8 llv8;

LLScan llscan;


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
  uint64_t total_size = 0;

  result.Printf(" Instances  Total Size Name\n");
  result.Printf(" ---------- ---------- ----\n");

  for (std::vector<TypeRecord*>::iterator it = sorted_by_count.begin();
       it != sorted_by_count.end(); ++it) {
    TypeRecord* t = *it;
    result.Printf(" %10" PRId64 " %10" PRId64 " %s\n", t->GetInstanceCount(),
                  t->GetTotalInstanceSize(), t->GetTypeName().c_str());
    total_objects += t->GetInstanceCount();
    total_size += t->GetTotalInstanceSize();
  }

  result.Printf(" ---------- ---------- \n");
  result.Printf(" %10" PRId64 " %10" PRId64 " \n", total_objects, total_size);

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


bool FindInstancesCmd::DoExecute(SBDebugger d, char** cmd,
                                 SBCommandReturnObject& result) {
  if (cmd == nullptr || *cmd == nullptr) {
    result.SetError("USAGE: v8 findjsinstances [flags] instance_name\n");
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
  if (cmd == nullptr || *cmd == nullptr) {
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

  if (!scanner->AreReferencesLoaded()) {
    ScanForReferences(scanner);
  }
  ReferencesVector* references = scanner->GetReferences();
  PrintReferences(result, references, scanner);

  delete scanner;

  result.SetStatus(eReturnStatusSuccessFinishResult);
  return true;
}


void FindReferencesCmd::ScanForReferences(ObjectScanner* scanner) {
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
      if (v8::JSObject::IsObjectType(v8, type) ||
          type == v8->types()->kJSArrayType) {
        // Objects can have elements and arrays can have named properties.
        // Basically we need to access objects and arrays as both objects and
        // arrays.
        v8::JSObject js_obj(heap_object);
        scanner->ScanRefs(js_obj, err);

      } else if (type < v8->types()->kFirstNonstringType) {
        v8::String str(heap_object);
        scanner->ScanRefs(str, err);

      } else if (type == v8->types()->kJSTypedArrayType) {
        // These should only point to off heap memory,
        // this case should be a no-op.
      } else {
        // result.Printf("Unhandled type: %" PRId64 " for addr %" PRIx64
        //    "\n", type, addr);
      }
    }
  }
}


void FindReferencesCmd::PrintReferences(SBCommandReturnObject& result,
                                        ReferencesVector* references,
                                        ObjectScanner* scanner) {
  // Walk all the object instances and handle them according to their type.
  TypeRecordMap mapstoinstances = llscan.GetMapsToInstances();
  for (uint64_t addr : *references) {
    v8::Error err;
    v8::Value obj_value(&llv8, addr);
    v8::HeapObject heap_object(obj_value);
    int64_t type = heap_object.GetType(err);
    v8::LLV8* v8 = heap_object.v8();

    // We only need to handle the types that are in
    // FindJSObjectsVisitor::IsAHistogramType
    // as those are the only objects that end up in GetMapsToInstances
    if (v8::JSObject::IsObjectType(v8, type) ||
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
  optind = 0;
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

  return &cmd[optind - 1];
}


void FindReferencesCmd::ReferenceScanner::PrintRefs(
    SBCommandReturnObject& result, v8::JSObject& js_obj, v8::Error& err) {
  int64_t length = js_obj.GetArrayLength(err);
  for (int64_t i = 0; i < length; ++i) {
    v8::Value v = js_obj.GetArrayElement(i, err);

    // Array is borked, or not array at all - skip it
    if (!err.Success()) break;

    if (v.raw() != search_value_.raw()) continue;

    std::string type_name = js_obj.GetTypeName(err);
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
      std::string type_name = js_obj.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", js_obj.raw(),
                    type_name.c_str(), key.c_str(), search_value_.raw());
    }
  }
}


void FindReferencesCmd::ReferenceScanner::PrintRefs(
    SBCommandReturnObject& result, v8::String& str, v8::Error& err) {
  v8::LLV8* v8 = str.v8();

  int64_t repr = str.Representation(err);

  // Concatenated and sliced strings refer to other strings so
  // we need to check their references.

  if (repr == v8->string()->kSlicedStringTag) {
    v8::SlicedString sliced_str(str);
    v8::String parent = sliced_str.Parent(err);
    if (err.Success() && parent.raw() == search_value_.raw()) {
      std::string type_name = sliced_str.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<Parent>", search_value_.raw());
    }
  } else if (repr == v8->string()->kConsStringTag) {
    v8::ConsString cons_str(str);

    v8::String first = cons_str.First(err);
    if (err.Success() && first.raw() == search_value_.raw()) {
      std::string type_name = cons_str.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<First>", search_value_.raw());
    }

    v8::String second = cons_str.Second(err);
    if (err.Success() && second.raw() == search_value_.raw()) {
      std::string type_name = cons_str.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<Second>", search_value_.raw());
    }
  } else if (repr == v8->string()->kThinStringTag) {
    v8::ThinString thin_str(str);
    v8::String actual = thin_str.Actual(err);
    if (err.Success() && actual.raw() == search_value_.raw()) {
      std::string type_name = thin_str.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", str.raw(),
                    type_name.c_str(), "<Actual>", search_value_.raw());
    }
  }
  // Nothing to do for other kinds of string.
}


void FindReferencesCmd::ReferenceScanner::ScanRefs(v8::JSObject& js_obj,
                                                   v8::Error& err) {
  ReferencesVector* references;
  std::set<uint64_t> already_saved;

  int64_t length = js_obj.GetArrayLength(err);
  for (int64_t i = 0; i < length; ++i) {
    v8::Value v = js_obj.GetArrayElement(i, err);

    // Array is borked, or not array at all - skip it
    if (!err.Success()) break;
    if (already_saved.count(v.raw())) continue;

    references = llscan.GetReferencesByValue(v.raw());
    references->push_back(js_obj.raw());
    already_saved.insert(v.raw());
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

    if (already_saved.count(v.raw())) continue;

    references = llscan.GetReferencesByValue(v.raw());
    references->push_back(js_obj.raw());
    already_saved.insert(v.raw());
  }
}


void FindReferencesCmd::ReferenceScanner::ScanRefs(v8::String& str,
                                                   v8::Error& err) {
  ReferencesVector* references;
  std::set<uint64_t> already_saved;

  v8::LLV8* v8 = str.v8();

  int64_t repr = str.Representation(err);

  // Concatenated and sliced strings refer to other strings so
  // we need to check their references.

  if (repr == v8->string()->kSlicedStringTag) {
    v8::SlicedString sliced_str(str);
    v8::String parent = sliced_str.Parent(err);

    if (err.Success()) {
      references = llscan.GetReferencesByValue(parent.raw());
      references->push_back(str.raw());
    }

  } else if (repr == v8->string()->kConsStringTag) {
    v8::ConsString cons_str(str);

    v8::String first = cons_str.First(err);
    if (err.Success()) {
      references = llscan.GetReferencesByValue(first.raw());
      references->push_back(str.raw());
    }

    v8::String second = cons_str.Second(err);
    if (err.Success() && first.raw() != second.raw()) {
      references = llscan.GetReferencesByValue(second.raw());
      references->push_back(str.raw());
    }
  } else if (repr == v8->string()->kThinStringTag) {
    v8::ThinString thin_str(str);
    v8::String actual = thin_str.Actual(err);

    if (err.Success()) {
      references = llscan.GetReferencesByValue(actual.raw());
      references->push_back(str.raw());
    }
  }
  // Nothing to do for other kinds of string.
}


bool FindReferencesCmd::ReferenceScanner::AreReferencesLoaded() {
  return llscan.AreReferencesByValueLoaded();
}


ReferencesVector* FindReferencesCmd::ReferenceScanner::GetReferences() {
  return llscan.GetReferencesByValue(search_value_.raw());
}


void FindReferencesCmd::PropertyScanner::PrintRefs(
    SBCommandReturnObject& result, v8::JSObject& js_obj, v8::Error& err) {
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
      std::string type_name = js_obj.GetTypeName(err);
      result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 "\n", js_obj.raw(),
                    type_name.c_str(), key.c_str(), entry.second.raw());
    }
  }
}


void FindReferencesCmd::PropertyScanner::ScanRefs(v8::JSObject& js_obj,
                                                  v8::Error& err) {
  // (Note: We skip array elements as they don't have names.)

  // Walk all the properties in this object.
  // We only create strings for the field names that match the search
  // value.
  ReferencesVector* references;
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
    references = llscan.GetReferencesByProperty(key);
    references->push_back(js_obj.raw());
  }
}


bool FindReferencesCmd::PropertyScanner::AreReferencesLoaded() {
  return llscan.AreReferencesByPropertyLoaded();
}


ReferencesVector* FindReferencesCmd::PropertyScanner::GetReferences() {
  return llscan.GetReferencesByProperty(search_value_);
}


void FindReferencesCmd::StringScanner::PrintRefs(SBCommandReturnObject& result,
                                                 v8::JSObject& js_obj,
                                                 v8::Error& err) {
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
        std::string type_name = js_obj.GetTypeName(err);

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
          std::string type_name = js_obj.GetTypeName(err);
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
      std::string type_name = sliced_str.GetTypeName(err);
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
        std::string type_name = cons_str.GetTypeName(err);
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
        std::string type_name = cons_str.GetTypeName(err);
        result.Printf("0x%" PRIx64 ": %s.%s=0x%" PRIx64 " '%s'\n", str.raw(),
                      type_name.c_str(), "<Second>", second_str.raw(),
                      second.c_str());
      }
    }
  }
  // Nothing to do for other kinds of string.
  // They are strings so we will find references to them.
}


void FindReferencesCmd::StringScanner::ScanRefs(v8::JSObject& js_obj,
                                                v8::Error& err) {
  v8::LLV8* v8 = js_obj.v8();
  ReferencesVector* references;
  std::set<std::string> already_saved;

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

      if (already_saved.count(value)) continue;

      references = llscan.GetReferencesByString(value);
      references->push_back(js_obj.raw());
      already_saved.insert(value);
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
        if (already_saved.count(value)) continue;

        references = llscan.GetReferencesByString(value);
        references->push_back(js_obj.raw());
        already_saved.insert(value);
      }
    }
  }
}


void FindReferencesCmd::StringScanner::ScanRefs(v8::String& str,
                                                v8::Error& err) {
  v8::LLV8* v8 = str.v8();
  ReferencesVector* references;

  // Concatenated and sliced strings refer to other strings so
  // we need to check their references.

  int64_t repr = str.Representation(err);
  if (err.Fail()) return;

  if (repr == v8->string()->kSlicedStringTag) {
    v8::SlicedString sliced_str(str);
    v8::String parent_str = sliced_str.Parent(err);
    if (err.Fail()) return;
    std::string parent = parent_str.ToString(err);
    if (err.Success()) {
      references = llscan.GetReferencesByString(parent);
      references->push_back(str.raw());
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

      if (err.Success()) {
        references = llscan.GetReferencesByString(first);
        references->push_back(str.raw());
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

      if (err.Success()) {
        references = llscan.GetReferencesByString(second);
        references->push_back(str.raw());
      }
    }
  }
  // Nothing to do for other kinds of string.
  // They are strings so we will find references to them.
}


bool FindReferencesCmd::StringScanner::AreReferencesLoaded() {
  return llscan.AreReferencesByStringLoaded();
}


ReferencesVector* FindReferencesCmd::StringScanner::GetReferences() {
  return llscan.GetReferencesByString(search_value_);
}


bool V8SnapshotCmd::DoExecute(SBDebugger d, char** cmd,
                              SBCommandReturnObject& result) {
  v8::Error err;
  writer_.open("core.heapsnapshot");

  SBTarget target = d.GetSelectedTarget();
  if (!target.IsValid()) {
    result.SetError("No valid process, please start something\n");
    return false;
  }

  // Load V8 constants from postmortem data
  llv8.Load(target);

  /* Ensure we have a map of objects. */
  if (!llscan.ScanHeapForObjects(target, result)) {
    result.SetStatus(eReturnStatusFailed);
    return false;
  }

  PrepareData(err);
  if (err.Fail()) {
    std::cout << "wow" << std::endl;
    return false;
  }

  SerializeImpl(err);
  if (err.Fail()) {
    std::cout << "WOW" << std::endl;
  }

  writer_.close();
  return !err.Fail();
}


V8SnapshotCmd::Node::Type V8SnapshotCmd::GetInstanceType(
    v8::Error &err, uint64_t word) {

  v8::Value v8_value(&llv8, word);

  // Test if this is SMI
  v8::Smi smi(v8_value);
  if (smi.Check()) return Node::Type::kSimdValue;

  v8::HeapObject heap_object(v8_value);
  if (!heap_object.Check()) return Node::Type::kInvalid;

  int64_t type = heap_object.GetType(err);

  if(type < llv8.types()->kFirstNonstringType) {
    return Node::Type::kString;
  }
  if (type == llv8.types()->kFixedArrayType ||
      type == llv8.types()->kJSArrayType ||
      type == llv8.types()->kJSArrayBufferType ||
      type == llv8.types()->kJSTypedArrayType) {
    return Node::Type::kArray;
  }
  if (v8::JSObject::IsObjectType(&llv8, type)) {
    return Node::Type::kObject;
  }
  if (type == llv8.types()->kCodeType) {
    return Node::Type::kCode;
  }
  if (type == llv8.types()->kJSRegExpType) {
    return Node::Type::kRegExp;
  }
  if (type == llv8.types()->kHeapNumberType) {

  }
  return Node::Type::kInvalid;
}

uint64_t V8SnapshotCmd::GetStringId(
    v8::Error &err, std::string name) {
  auto pos = std::distance(
      strings_.begin(), find(strings_.begin(), strings_.end(), name));

  uint64_t index;
  if(pos >= strings_.size()) {
    index = strings_.size();
    strings_.push_back(name);
  } else {
    index = pos;
  }

  return index + 1;  // +1 because of the dummy string
}

uint64_t V8SnapshotCmd::GetSelfSize(
    v8::Error &err, uint64_t word) {
  v8::Value v8_value(&llv8, word);

  // Test if this is SMI
  v8::Smi smi(v8_value);
  if (smi.Check()) return 4;

  v8::HeapObject heap_object(v8_value);
  if (!heap_object.Check()) return -1;

  v8::HeapObject map_object = heap_object.GetMap(err);
  if (err.Fail() || !map_object.Check()) return -1;

  v8::Map map(map_object);

  return map.InstanceSize(err);
}

uint64_t V8SnapshotCmd::ChildrenCount(
    v8::Error &err, uint64_t word) {
  uint64_t children = 0;
  v8::Value v8_value(&llv8, word);

  // Test if this is SMI
  v8::Smi smi(v8_value);
  if (smi.Check()) return 0;

  v8::HeapObject heap_object(v8_value);
  if (!heap_object.Check()) return 0;

  v8::HeapObject map_object = heap_object.GetMap(err);
  if (err.Fail() || !map_object.Check()) return 0;

  v8::Map map(map_object);
  v8::HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return 0;

  v8::DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return 0;

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return 0;

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return 0;

  v8::JSObject js_obj(heap_object);
  v8::HeapObject extra_properties_obj = js_obj.Properties(err);
  if (err.Fail()) return 0;

  v8::FixedArray extra_properties(extra_properties_obj);

  for (int64_t i = 0; i < own_descriptors_count; i++) {
    v8::Value value;
    v8::Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) continue;

    v8::Value key = descriptors.GetKey(i, err);
    if (err.Fail()) continue;

    if (descriptors.IsConstFieldDetails(details)) {
      value = descriptors.GetValue(i, err);
      if (err.Fail()) continue;
    } else if (!descriptors.IsFieldDetails(details)) {
    // Skip non-fields for now
      v8::Error::PrintInDebugMode("Unknown field Type %" PRId64,
                              details.GetValue());
      continue;
    } else {
      int64_t index = descriptors.FieldIndex(details) - in_object_count;
      if (descriptors.IsDoubleField(details)) {
        // NOTE (mmarchini) should we consider double fields too?
        #if 0
        double value;
        if (index < 0)
          value = js_obj.GetInObjectValue<double>(instance_size, index, err);
        else
          value = extra_properties.Get<double>(index, err);

        if (err.Fail()) return -1;

        addr = value
        #endif
        continue;
      } else {
        if (index < 0)
          value = js_obj.GetInObjectValue<v8::Value>(instance_size, index, err);
        else
          value = extra_properties.Get<v8::Value>(index, err);

        if (err.Fail()) continue;
      }
    }

    if (err.Fail()) continue;

    v8::Smi smi(value);
    if (smi.Check()) {
      continue;
    };

    v8::HeapObject obj(value);
    if (!obj.Check()) {
      // std::cout << "Not object and not smi" << std::endl;
      continue;
    }

    int64_t type = obj.GetType(err);
    if (err.Fail()) return false;

    if (type == llv8.types()->kOddballType) {
      // std::cout << "Oddball" << std::endl;
      continue;
    }

    if (type == llv8.types()->kJSFunctionType) {
      // std::cout << "Function" << std::endl;
      continue;
    }

    v8::Value::InspectOptions options;
    options.detailed = false;
    if (obj.Inspect(&options, err).find("<unknown>") != std::string::npos) {
      // std::cout << "unknown" << std::endl;
      continue;
    }

    auto scanner = new FindReferencesCmd::ReferenceScanner(value);
    if (!scanner->AreReferencesLoaded()) {
      FindReferencesCmd::ScanForReferences(scanner);
    }

    auto references = llscan.GetReferencesByValue(obj.raw());
    auto pos = std::distance(references->begin(),
        std::find(references->begin(), references->end(), word));

    if (pos >= references->size()) {
      // If the reference couldn't be found in the heap, skip
      continue;
    }

    Edge edge;
    edge.type = Edge::Type::kProperty;
    edge.index_or_property = GetStringId(err, key.ToString(err));
    edge.to_addr = obj.raw();
    // edge.to_addr = obj.raw();
    edges_.push_back(edge);
    children++;
  }

  return children;
}

void V8SnapshotCmd::PrepareData(v8::Error &err) {
  std::map<uint64_t, Node> visited;
  uint64_t next_id = 1;
  const int step = 2;

  // AddRootEntry
  {
    Node node;
    node.addr = 0;
    node.type = Node::Type::kSynthetic;
    node.name_id = GetStringId(err, "");
    node.node_id = 1;
    next_id += step;
    node.self_size = 0;
    node.children = 0;
  }

  // AddGCRootsEntry
  {
    Node node;
    node.addr = 0;
    node.type = Node::Type::kSynthetic;
    node.name_id = GetStringId(err, "(GC roots)");
    node.node_id = next_id;
    next_id += step;
    node.self_size = 0;
    node.children = 0;
  }

  // AddGcSubrootEntries

  // Prepare Nodes
  for(auto type_record : llscan.GetMapsToInstances()) {
    for(auto instance : type_record.second->GetInstances()) {
      if (visited.count(instance) != 0) {
        std::cout << "bad" << std::endl;
        return;
      }

      Node node;
      node.addr = instance;
      node.type = GetInstanceType(err, instance);
      if(node.type == Node::Type::kInvalid) {
        std::cout << "kInvalid: " << instance << std::endl;
        continue;
      }
      node.name_id = GetStringId(err, type_record.second->GetTypeName());
      node.node_id = next_id;
      next_id += step;
      node.self_size = GetSelfSize(err, instance);
      node.children = ChildrenCount(err, instance);
      if (node.children == -1) {
        std::cout << "-1 children: " << instance << std::endl;
        continue;
      }
      node.trace_node_id = 0;  // TODO (mmarchini) traces
      nodes_.push_back(node);
      visited.insert(
          std::pair<uint64_t, Node>(instance, nodes_.back()));
    }
  }

  int problems = 0;
  std::cout << edges_.size() << std::endl;
  // Prepare Edges
  for(auto& edge : edges_) {
    if (visited.count(edge.to_addr) != 1) {
      std::cout << "Edge was not visited: " << std::hex << edge.to_addr << std::endl;
      edge.to_id = -1;
      problems++;
      continue;
    }
    Node node = visited.at(edge.to_addr);

    if (node.addr != edge.to_addr) {
      std::cout << "Invalid edge: " << std::hex << edge.to_addr << " != ";
      std::cout << std::hex << node.addr << std::endl;
      edge.to_id = -1;
      problems++;
      continue;
    }
    // std::cout << "worked" << std::endl;
    edge.to_id = node.node_id * 6;
  }
  std::cout << "Problems found: " << std::dec << problems << std::endl;
}


bool V8SnapshotCmd::SerializeImpl(v8::Error &err) {
//
  writer_ << "{";
  writer_ << "\"snapshot\":{";
  SerializeSnapshot(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "}," << std::endl;
  writer_ << "\"nodes\":[";
  SerializeNodes(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]," << std::endl;
  writer_ << "\"edges\":[";
  SerializeEdges(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]," << std::endl;

  writer_ << "\"trace_function_infos\":[";
  SerializeTraceNodeInfos(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]," << std::endl;
  writer_ << "\"trace_tree\":[";
  SerializeTraceTree(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]," << std::endl;

  writer_ << "\"samples\":[";
  SerializeSamples(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]," << std::endl;

  writer_ << "\"strings\":[";
  SerializeStrings(err);  // TODO incomplete
  if (err.Fail()) return false;
  writer_ << "]";
  writer_ << "}";

  return true;
}


void V8SnapshotCmd::SerializeSnapshot(v8::Error &err) {
  writer_ << "\"meta\":";
  // The object describing node serialization layout.
  // We use a set of macros to improve readability.
#define JSON_A(s) "[" s "]"
#define JSON_O(s) "{" s "}"
#define JSON_S(s) "\"" s "\""
  writer_ << JSON_O(
    JSON_S("node_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name") ","
        JSON_S("id") ","
        JSON_S("self_size") ","
        JSON_S("edge_count") ","
        JSON_S("trace_node_id")) ","
    JSON_S("node_types") ":" JSON_A(
        JSON_A(
            JSON_S("hidden") ","
            JSON_S("array") ","
            JSON_S("string") ","
            JSON_S("object") ","
            JSON_S("code") ","
            JSON_S("closure") ","
            JSON_S("regexp") ","
            JSON_S("number") ","
            JSON_S("native") ","
            JSON_S("synthetic") ","
            JSON_S("concatenated string") ","
            JSON_S("sliced string")) ","
        JSON_S("string") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number")) ","
    JSON_S("edge_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name_or_index") ","
        JSON_S("to_node")) ","
    JSON_S("edge_types") ":" JSON_A(
        JSON_A(
            JSON_S("context") ","
            JSON_S("element") ","
            JSON_S("property") ","
            JSON_S("internal") ","
            JSON_S("hidden") ","
            JSON_S("shortcut") ","
            JSON_S("weak")) ","
        JSON_S("string_or_number") ","
        JSON_S("node")) ","
    JSON_S("trace_function_info_fields") ":" JSON_A(
        JSON_S("function_id") ","
        JSON_S("name") ","
        JSON_S("script_name") ","
        JSON_S("script_id") ","
        JSON_S("line") ","
        JSON_S("column")) ","
    JSON_S("trace_node_fields") ":" JSON_A(
        JSON_S("id") ","
        JSON_S("function_info_index") ","
        JSON_S("count") ","
        JSON_S("size") ","
        JSON_S("children")) ","
    JSON_S("sample_fields") ":" JSON_A(
        JSON_S("timestamp_us") ","
        JSON_S("last_assigned_id")));
#undef JSON_S
#undef JSON_O
#undef JSON_A
  writer_ << ",\"node_count\":";
  writer_ << nodes_.size();

  writer_ << ",\"edge_count\":";
  writer_ << edges_.size();

  // TODO (mmarchini): allocator tracker
  // AllocationTracker* tracker = snapshot_->profiler()->allocation_tracker();
  // if (tracker) {
  //   count = static_cast<uint32_t>(tracker->function_info_list().size());
  // }
  writer_ << ",\"trace_function_count\":";
  uint32_t count = 0;
  writer_ << count;
}

void V8SnapshotCmd::SerializeNodes(v8::Error &err) {
  bool first_node = true;
  for(auto node : nodes_) {
    SerializeNode(err, &node, first_node);
    if (err.Fail()) return;
    first_node = false;
  }
}


void V8SnapshotCmd::SerializeNode(v8::Error &err, Node* node, bool first_node) {
  if (!first_node) {
    writer_ <<  ',';
  }
  writer_ << node->type << "," <<
      node->name_id << "," <<
      node->node_id << "," <<
      node->self_size << "," <<
      node->children << "," <<
      node->trace_node_id << std::endl;
}


void V8SnapshotCmd::SerializeEdges(v8::Error &err) {
  bool first_edge = true;
  for(auto edge : edges_) {
    SerializeEdge(err, edge, first_edge);
    if (err.Fail()) return;
    first_edge = false;
  }
}


void V8SnapshotCmd::SerializeEdge(v8::Error &err, Edge edge, bool first_edge) {
  if (!first_edge) {
    writer_ <<  ',';
  }
  writer_ << edge.type << "," << edge.index_or_property << "," << edge.to_id
      << std::endl;
}


void V8SnapshotCmd::SerializeTraceNodeInfos(v8::Error &err) {
  #if 0
  int i = 0;
  // TODO I don't have the slightest idea of what I should be iterating on
  for (auto it : llscan.GetMapsToInstances()) {
    if (i++ > 0) {
      writer_ << ',';
    }

    auto function_id = 0;  // info->function_id
    auto name = 0;  // GetStringId(info->name)
    auto script_name = 0;  // GetStringId(info->script_name)
    auto script_id = 0;  // static_cast<unsigned>(info->script_id)
    auto line = 0;  // info->line
    auto column = 0;  // info->column

    writer_ << function_id << "," << name << "," << script_name << "," <<
        script_id << "," << line << "," << column << std::endl;
  }
  #endif
}


void V8SnapshotCmd::SerializeTraceTree(v8::Error &err) {
  #if 0
  // AllocationTracker* tracker = snapshot_->profiler()->allocation_tracker();
  // if (!tracker) return;
  // AllocationTraceTree* traces = tracker->trace_tree();
  SerializeTraceNode(err);
  #endif
}


// void HeapSnapshotJSONSerializer::SerializeTraceNode(AllocationTraceNode* node) {
void V8SnapshotCmd::SerializeTraceNode(v8::Error &err) {
  #if 0
  auto node_id = 0;  // node->id()
  auto node_function_info_index = 0;  // node->function_info_index()
  auto node_allocation_count = 0;  // node->allocation_count()
  auto node_allocation_size = 0;  // node->allocation_size()

  writer_ << node_id << "," << node_function_info_index << ","
      << node_allocation_count << "," << node_allocation_size << "," << "[";


  // int i = 0;
  // for (AllocationTraceNode* child : node->children()) {
  //   if (i++ > 0) {
  //     writer_->AddCharacter(',');
  //   }
  //   SerializeTraceNode(child);
  // }
  writer_ << ']';
  #endif
}


void V8SnapshotCmd::SerializeSamples(v8::Error &err) {
  #if 0
  // const std::vector<HeapObjectsMap::TimeInterval>& samples =
  //     snapshot_->profiler()->heap_object_map()->samples();
  // if (samples.empty()) return;
  // base::TimeTicks start_time = samples[0].timestamp;
  // // The buffer needs space for 2 unsigned ints, 2 commas, \n and \0
  // const int kBufferSize = MaxDecimalDigitsIn<sizeof(
  //                             base::TimeDelta().InMicroseconds())>::kUnsigned +
  //                         MaxDecimalDigitsIn<sizeof(samples[0].id)>::kUnsigned +
  //                         2 + 1 + 1;
  // EmbeddedVector<char, kBufferSize> buffer;
  int i = 0;
  // for (const HeapObjectsMap::TimeInterval& sample : samples) {
  for (auto it : {1, 2, 3}) {
    if (i++ > 0) {
      writer_ << ',';
    }

    auto time_delta = 0;  // time_delta.InMicroseconds()
    auto sample_last_assigned_id = 0;  // sample.last_assigned_id()

    writer_ << time_delta << "," << sample_last_assigned_id << std::endl;
  }
  #endif
}


void V8SnapshotCmd::SerializeStrings(v8::Error &err) {
  writer_ << "\"<dummy>\"";
  for (auto s : strings_) {
    writer_ << ',';
    SerializeString(err, s.c_str());
    if (err.Fail()) return;
  }
}


void V8SnapshotCmd::SerializeString(v8::Error &err, std::string s) {
  writer_ << '\n';
  writer_ << '\"';
  writer_ << s;
  #if 0
  for ( ; *s != '\0'; ++s) {
    switch (*s) {
      case '\b':
        writer_ << "\\b";
        continue;
      case '\f':
        writer_ << "\\f";
        continue;
      case '\n':
        writer_ << "\\n";
        continue;
      case '\r':
        writer_ << "\\r";
        continue;
      case '\t':
        writer_ << "\\t";
        continue;
      case '\"':
      case '\\':
        writer_ << '\\';
        writer_ << *s;
        continue;
      default:
        if (*s > 31 && *s < 128) {
          writer_ << *s;
        // } else if (*s <= 31) {
        //   // Special character with no dedicated literal.
        //   WriteUChar(writer_, *s);
        // } else {
        //   // Convert UTF-8 into \u UTF-16 literal.
        //   size_t length = 1, cursor = 0;
        //   for ( ; length <= 4 && *(s + length) != '\0'; ++length) { }
        //   unibrow::uchar c = unibrow::Utf8::CalculateValue(s, length, &cursor);
        //   if (c != unibrow::Utf8::kBadChar) {
        //     WriteUChar(writer_, c);
        //     DCHECK(cursor != 0);
        //     s += cursor - 1;
        //   } else {
        //     writer_->AddCharacter('?');
        //   }
        }
    }
  }
  #endif
  writer_ << '\"';
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
    // Check type first
    map_info.is_histogram = IsAHistogramType(map, err);

    // On success load type name
    if (map_info.is_histogram)
      map_info.type_name = heap_object.GetTypeName(err);

    // Cache result
    map_cache_.emplace(map.raw(), map_info);

    if (err.Fail()) return address_byte_size_;
  } else {
    map_info = map_cache_.at(map.raw());
  }

  if (!map_info.is_histogram) return address_byte_size_;

  /* No entry in the map, create a new one. */
  if (mapstoinstances_.count(map_info.type_name) == 0) {
    TypeRecord* t = new TypeRecord(map_info.type_name);

    t->AddInstance(word, map.InstanceSize(err));
    mapstoinstances_.emplace(map_info.type_name, t);

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
  if (v8::JSObject::IsObjectType(v8, type)) return true;
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
    ClearMapsToInstances();
    ClearReferences();
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

inline static ByteOrder GetHostByteOrder() {
  union {
    uint8_t a[2];
    uint16_t b;
  } u = {{0, 1}};
  return u.b == 1 ? ByteOrder::eByteOrderBig : ByteOrder::eByteOrderLittle;
}

void LLScan::ScanMemoryRanges(FindJSObjectsVisitor& v) {
  bool done = false;

  const uint64_t addr_size = process_.GetAddressByteSize();
  bool swap_bytes = process_.GetByteOrder() != GetHostByteOrder();

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
  lldb::SBMemoryRegionInfoList memory_regions = process_.GetMemoryRegions();
  lldb::SBMemoryRegionInfo region_info;

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

        if (addr_size == 4) {
          value = *reinterpret_cast<uint32_t*>(&block[j]);
          if (swap_bytes) {
            value = __builtin_bswap32(value);
          }
        } else if (addr_size == 8) {
          value = *reinterpret_cast<uint64_t*>(&block[j]);
          if (swap_bytes) {
            value = __builtin_bswap64(value);
          }
        } else {
          break;
        }

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
  TypeRecord* t;
  for (auto entry : mapstoinstances_) {
    t = entry.second;
    delete t;
  }
  mapstoinstances_.clear();
}

void LLScan::ClearReferences() {
  ReferencesVector* references;

  for (auto entry : references_by_value_) {
    references = entry.second;
    delete references;
  }
  references_by_value_.clear();

  for (auto entry : references_by_property_) {
    references = entry.second;
    delete references;
  }
  references_by_property_.clear();

  for (auto entry : references_by_string_) {
    references = entry.second;
    delete references;
  }
  references_by_string_.clear();
}
}  // namespace llnode
