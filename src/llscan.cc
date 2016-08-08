#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
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
  }

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


FindJSObjectsVisitor::FindJSObjectsVisitor(SBTarget& target,
                                           TypeRecordMap& mapstoinstances)
    : target_(target), mapstoinstances_(mapstoinstances) {
  found_count_ = 0;
  address_byte_size_ = target_.GetProcess().GetAddressByteSize();
  // Load V8 constants from postmortem data
  llv8.Load(target);
}


/* Visit every address, a bit brute force but it works. */
uint64_t FindJSObjectsVisitor::Visit(uint64_t location, uint64_t available) {
  lldb::SBError error;

  // Test if the map points to a real map.
  // Try to create an object out of it.

  uint64_t word = target_.GetProcess().ReadUnsignedFromMemory(
      location, address_byte_size_, error);

  v8::Value v8_value(&llv8, word);

  v8::Error err;
  // Test if this is SMI
  // Skip inspecting things that look like Smi's, they aren't objects.
  v8::Smi smi(v8_value);
  if (smi.Check()) {
    return address_byte_size_;
  }

  v8::HeapObject heap_object(v8_value);
  if (!heap_object.Check()) {
    return address_byte_size_;
  }
  if (heap_object.IsHoleOrUndefined(err)) {
    return address_byte_size_;
  }
  if (err.Fail()) {
    return address_byte_size_;
  }

  v8::HeapObject map_object = heap_object.GetMap(err);
  if (err.Fail() || !map_object.Check()) {
    return address_byte_size_;
  }

  if (!IsAHistogramType(heap_object, err)) {
    return address_byte_size_;
  }

  if (err.Fail()) {
    return address_byte_size_;
  }

  v8::Map map(map_object);

  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = false;
  inspect_options.print_map = false;
  inspect_options.string_length = 0;

  std::string type_name = heap_object.GetTypeName(&inspect_options, err);

  /* No entry in the map, create a new one. */
  if (mapstoinstances_.count(type_name) == 0) {
    TypeRecord* t = new TypeRecord(type_name);

    t->AddInstance(word, map.InstanceSize(err));
    mapstoinstances_.insert(std::pair<std::string, TypeRecord*>(type_name, t));

  } else {
    /* Update an existing instance, if we haven't seen this instance before. */
    TypeRecord* t = mapstoinstances_.at(type_name);
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


bool FindJSObjectsVisitor::IsAHistogramType(v8::HeapObject& heap_object,
                                            v8::Error err) {
  int64_t type = heap_object.GetType(err);
  v8::LLV8* v8 = heap_object.v8();
  if (type == v8->types()->kJSObjectType) return true;
  if (type == v8->types()->kJSArrayType) return true;
  if (type == v8->types()->kJSTypedArrayType) return true;
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
    for (auto searchAddress = address; searchAddress < address + len;) {
      uint32_t increment =
          v.Visit(searchAddress, (address + len) - searchAddress);
      if (increment == 0) {
        done = true;
        break;
      }
      searchAddress += increment;
    }
  }
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
