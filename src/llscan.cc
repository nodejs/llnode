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


bool ListObjectsCmd::DoExecute(SBDebugger d, char** cmd,
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
  std::map<uint64_t, TypeRecord*>::iterator end =
      llscan.GetMapsToInstances().end();
  for (std::map<uint64_t, TypeRecord*>::iterator it =
           llscan.GetMapsToInstances().begin();
       it != end; ++it) {
    sorted_by_count.push_back(it->second);
  }

  std::sort(sorted_by_count.begin(), sorted_by_count.end(),
            TypeRecord::compareInstanceCounts);

  uint64_t total_objects = 0;

  result.Printf("Map                Instances  Total Size Name\n");
  result.Printf("------------------ ---------- ---------- ----\n");

  for (std::vector<TypeRecord*>::iterator it = sorted_by_count.begin();
       it != sorted_by_count.end(); ++it) {
    TypeRecord* t = *it;
    result.Printf("0x%016" PRIx64 " %10" PRId64 " %10" PRId64 " %s\n", t->map,
                  t->instance_count, t->total_instance_size,
                  t->type_name->c_str());
    total_objects += t->instance_count;
  }

  return true;
}


/* Utility function to generate short type names for objects.
 * Based on the inspection code in llv8.cc
 * TODO(indutny) - Should this merge back in to llv8.cc?
 */
std::string GetTypeName(v8::HeapObject& heap_object,
                        v8::Value::InspectOptions& options, v8::Error err) {
  int64_t type = heap_object.GetType(err);
  v8::LLV8* v8 = heap_object.v8();
  if (type == v8->types()->kGlobalObjectType)
    return heap_object.Inspect(&options, err);
  if (type == v8->types()->kCodeType) return heap_object.Inspect(&options, err);
  if (type == v8->types()->kMapType) {
    return heap_object.Inspect(&options, err);
  }

  if (type == v8->types()->kJSObjectType) {
    v8::HeapObject map_obj = heap_object.GetMap(err);
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

    if (constructor_type != v8->types()->kJSFunctionType) {
      return "<Object: no constructor>";
    }

    v8::JSFunction constructor(constructor_obj);

    return constructor.Name(err);
  }

  if (type == v8->types()->kHeapNumberType) {
    return "(HeapNumber)";
  }

  if (type == v8->types()->kJSArrayType) {
    return "(Array)";
  }

  if (type == v8->types()->kOddballType) {
    return "(Oddball)";
  }

  if (type == v8->types()->kJSFunctionType) {
    return "(Function)";
  }

  if (type == v8->types()->kJSRegExpType) {
    return "(RegExp)";
  }

  if (type < v8->types()->kFirstNonstringType) {
    return "(String)";
  }

  if (type == v8->types()->kFixedArrayType) {
    return "(FixedArray)";
  }

  if (type == v8->types()->kJSArrayBufferType) {
    return "(ArrayBuffer)";
  }

  if (type == v8->types()->kJSTypedArrayType) {
    return "(ArrayBufferView)";
  }

  if (type == v8->types()->kJSDateType) {
    return "(Date)";
  }

  std::string unknown("unknown: ");

  return unknown += std::to_string(type);
}


bool ListInstancesCmd::DoExecute(SBDebugger d, char** cmd,
                                 SBCommandReturnObject& result) {
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

  SBExpressionOptions options;
  SBValue value = target.EvaluateExpression(full_cmd.c_str(), options);
  if (value.GetError().Fail()) {
    SBStream desc;
    if (!value.GetError().GetDescription(desc)) return false;
    result.SetError(desc.GetData());
    return false;
  }

  // Load V8 constants from postmortem data
  llv8.Load(target);

  v8::Value v8_mapaddr(&llv8, value.GetValueAsSigned());
  v8::Error err;

  if (err.Fail()) {
    result.SetError("Failed to evaluate expression");
    return false;
  }

  uint64_t mapaddr = value.GetValueAsUnsigned();
  std::map<uint64_t, TypeRecord*>::iterator instance_it =
      llscan.GetMapsToInstances().find(mapaddr);
  if (instance_it != llscan.GetMapsToInstances().end()) {
    TypeRecord* t = instance_it->second;
    for (std::set<uint64_t>::iterator it = t->instances->begin();
         it != t->instances->end(); ++it) {
      v8::Error err;
      v8::Value v8_value(&llv8, *it);
      std::string res = v8_value.Inspect(&inspect_options, err);
      result.Printf("%s\n", res.c_str());
    }

  } else {
    result.Printf("No objects found with map %" PRIx64 "\n", mapaddr);
  }

  return true;
}


FindJSObjectsVisitor::FindJSObjectsVisitor(
    SBTarget& target, std::map<uint64_t, TypeRecord*>& mapstoinstances)
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
      location, target_.GetProcess().GetAddressByteSize(), error);

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

  /* No entry in the map, create a new one. */
  if (mapstoinstances_.count(map_object.raw()) == 0) {
    TypeRecord* t = new TypeRecord();
    t->map = map_object.raw();
    t->instance_count++;
    // TODO(indutny) - Is this correct or is there a better way to find the size
    // of one instance of an object?
    t->total_instance_size = map.InstanceSize(err);

    t->type_name =
        new std::string(GetTypeName(heap_object, inspect_options, err));

    mapstoinstances_.insert(
        std::pair<uint64_t, TypeRecord*>(map_object.raw(), t));
    t->instances = new std::set<uint64_t>();
  } else {
    /* Update an existing instance, if we haven't seen this one before. */
    TypeRecord* t = mapstoinstances_.at(map_object.raw());
    /* Determine if this is a new instance.
     * (We are scanning pointers to objects, we may have seen this location
     * before.)
     */
    if (t->instances->count(word) == 0) {
      t->instances->insert(word);
      t->instance_count++;
      // TODO(indutny) - Is this correct or is there a better way to find the
      // size of one instance of an object?
      t->total_instance_size += map.InstanceSize(err);
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
                                lldb::SBCommandReturnObject result) {
  /* TODO(hhellyer) - Check whether we have the SBGetMemoryRegionInfoList API
   * available
   * and implemented.
   * (It may be available but not supported on this platform.)
   * If it is then check the last scan is still valid - the process hasn't moved
   * and we haven't changed target.
   */

  // Reload process anyway
  process_ = target.GetProcess();

  // Need to reload memory ranges (though this does assume the user has also
  // updated
  // RANGESFILE with data for the new dump or things won't match up).
  if (target_ != target) {
    MemoryRange* head = ranges_;
    while (head != nullptr) {
      MemoryRange* range = head;
      head = head->next;
      delete range;
    }
    ranges_ = nullptr;
  }

  /* Fall back to environment variable containing pre-parsed list of memory
   * ranges. */
  if (nullptr == ranges_) {
    const char* segmentsfilename = getenv("RANGESFILE");

    //
    if (segmentsfilename == nullptr) {
      //		  result.Printf("No memory ranges file. Plugin commands
      // that need to scan addressable memory will fail.\n");
      result.SetError(
          "No memory range information available for this process. Cannot scan "
          "for objects.\n");
      return false;
    }

    if (!GenerateMemoryRanges(target, segmentsfilename)) {
      result.SetError(
          "No memory range information available for this process. Cannot scan "
          "for objects.\n");
      return false;
    }
  }

  /* If we've reached here we have access to information about the valid memory
   * ranges in the
   * process and can scan for objects.
   */

  /* Populate the map of objects. */
  if (mapstoinstances_.empty()) {
    FindJSObjectsVisitor v(target, GetMapsToInstances());

    ScanMemoryRanges(v);

    result.Printf("Did search of memory - found %d pointers)\n",
                  v.FoundCount());

  } else {
    result.Printf("Re-using cached map\n");
  }

  return true;
}


void LLScan::ScanMemoryRanges(FindJSObjectsVisitor& v) {
  MemoryRange* head = ranges_;

  bool done = false;

  while (head != nullptr && !done) {
    uint64_t address = head->start;
    uint64_t len = head->length;
    head = head->next;

    /* Brute force search - query every address - but allow the visitor code to
     * say
     * how far to move on so we don't read every byte.
     */
    for (auto searchAddress = address; searchAddress < address + len;) {
      //		  result.Printf("Checking for needle at: 0x%" PRIx64
      //"\n", searchAddress);
      //			  printf("Visiting address 0x%" PRIx64 "\n",
      // searchAddress);
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
 * There are scripts for generating this file on mac and linux stored as a
 * snippet on github:
 * https://gist.github.com/hhellyer/6d79009e142d6eef696525afe1ecea43
 * export the name or full path to the ranges file in the RANGESFILE env var
 * before starting
 * lldb and loading the llnode plugin.
 */
bool LLScan::GenerateMemoryRanges(lldb::SBTarget target,
                                  const char* segmentsfilename) {
  std::ifstream input(segmentsfilename);

  if (!input.is_open()) {
    return false;
  }

  uint64_t address = 0, len = 0;

  MemoryRange** tailptr = &ranges_;

  lldb::addr_t address_byte_size = target.GetProcess().GetAddressByteSize();

  while (input >> std::hex >> address >> std::hex >> len) {
    /* Check if the range is accessible.
     * The structure of a core file means if you check the start and the end of
     * a range then the
     * middle will be there, ranges are contiguous in the file, but cores often
     * get truncated due
     * to file size limits so ranges can be missing or truncated. Sometimes
     * shared memory segments
     * are omitted so it's also possible an entire section could be missing from
     * the middle.
     */
    lldb::SBError error;
    target.GetProcess().ReadPointerFromMemory(address, error);
    if (error.Success()) {
      target.GetProcess().ReadPointerFromMemory(
          (address + len) - address_byte_size, error);

      if (error.Success()) {
        MemoryRange* newRange = new MemoryRange();
        if (nullptr == newRange) {
          return false;
        }
        newRange->start = address;
        newRange->length = len;
        newRange->next = nullptr;
        (*tailptr) = newRange;
        tailptr = &(newRange->next);

      } else {
        /* Could not access last word, skip. */
      }
    } else {
      /* Could not access first word, skip. */
    }
  }
  return true;
}
}
