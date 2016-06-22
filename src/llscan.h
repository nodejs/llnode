#ifndef SRC_LLSCAN_H_
#define SRC_LLSCAN_H_

#include <lldb/API/LLDB.h>
#include <map>
#include <set>

namespace llnode {

class ListObjectsCmd : public CommandBase {
 public:
  ~ListObjectsCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class ListInstancesCmd : public CommandBase {
 public:
  ~ListInstancesCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  bool detailed_;
};

class MemoryVisitor {
 public:
  virtual ~MemoryVisitor(){};

  virtual uint64_t Visit(uint64_t location, uint64_t available) { return 0; };
};

class TypeRecord {
 public:
  uint64_t map;
  std::string* type_name;
  uint64_t property_count;
  uint64_t instance_count;
  uint64_t total_instance_size;
  std::set<uint64_t>* instances;

  /* Sort records by instance count, use the other fields as tie breakers
   * to give consistent ordering.
   */
  static bool compareInstanceCounts(TypeRecord* a, TypeRecord* b) {
    if (a->instance_count == b->instance_count) {
      if (a->total_instance_size == b->total_instance_size) {
        return a->type_name < b->type_name;
      }
      return a->total_instance_size < b->total_instance_size;
    }
    return a->instance_count < b->instance_count;
  }
};

class FindJSObjectsVisitor : MemoryVisitor {
 public:
  FindJSObjectsVisitor(lldb::SBTarget& target,
                       std::map<uint64_t, TypeRecord*>& mapstoinstances);
  ~FindJSObjectsVisitor() {}

  uint64_t Visit(uint64_t location, uint64_t available);

  uint32_t FoundCount() { return found_count_; }

 private:
  bool IsAHistogramType(v8::HeapObject& heap_object, v8::Error err);

  lldb::SBTarget& target_;
  uint32_t address_byte_size_;
  uint32_t found_count_;

  std::map<uint64_t, TypeRecord*>& mapstoinstances_;
};


class LLScan {
 public:
  LLScan(){};

  std::string GetTypeName(v8::HeapObject& heap_object,
                          v8::Value::InspectOptions& options, v8::Error err);
  bool ScanHeapForObjects(lldb::SBTarget target,
                          lldb::SBCommandReturnObject result);
  bool GenerateMemoryRanges(lldb::SBTarget target,
                            const char* segmentsfilename);

  std::map<uint64_t, TypeRecord*>& GetMapsToInstances() {
    return mapstoinstances_;
  };

 private:
  void ScanMemoryRanges(FindJSObjectsVisitor& v);

  struct MemoryRange {
    uint64_t start;
    uint64_t length;
    MemoryRange* next;
  };

  lldb::SBTarget target_;
  lldb::SBProcess process_;
  MemoryRange* ranges_ = nullptr;
  std::map<uint64_t, TypeRecord*> mapstoinstances_;
};

}  // llnode


#endif  // SRC_LLSCAN_H_
