#ifndef SRC_LLSCAN_H_
#define SRC_LLSCAN_H_

#include <lldb/API/LLDB.h>
#include <map>
#include <set>

namespace llnode {

class FindObjectsCmd : public CommandBase {
 public:
  ~FindObjectsCmd() override{};

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class FindInstancesCmd : public CommandBase {
 public:
  ~FindInstancesCmd() override{};

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  bool detailed_;
};

class MemoryVisitor {
 public:
  virtual ~MemoryVisitor(){};

  virtual uint64_t Visit(uint64_t location, uint64_t available) = 0;
};

class TypeRecord {
 public:
  TypeRecord(std::string& type_name)
      : type_name_(type_name), instance_count_(0), total_instance_size_(0){};

  inline std::string& GetTypeName() { return type_name_; };
  inline uint64_t GetInstanceCount() { return instance_count_; };
  inline uint64_t GetTotalInstanceSize() { return total_instance_size_; };
  inline std::set<uint64_t>& GetInstances() { return instances_; };

  inline void AddInstance(uint64_t address, uint64_t size) {
    instances_.insert(address);
    instance_count_++;
    total_instance_size_ += size;
  };

  /* Sort records by instance count, use the other fields as tie breakers
   * to give consistent ordering.
   */
  static bool CompareInstanceCounts(TypeRecord* a, TypeRecord* b) {
    if (a->instance_count_ == b->instance_count_) {
      if (a->total_instance_size_ == b->total_instance_size_) {
        return a->type_name_ < b->type_name_;
      }
      return a->total_instance_size_ < b->total_instance_size_;
    }
    return a->instance_count_ < b->instance_count_;
  }


 private:
  std::string type_name_;
  uint64_t instance_count_;
  uint64_t total_instance_size_;
  std::set<uint64_t> instances_;
};

typedef std::map<std::string, TypeRecord*> TypeRecordMap;

class FindJSObjectsVisitor : MemoryVisitor {
 public:
  FindJSObjectsVisitor(lldb::SBTarget& target, TypeRecordMap& mapstoinstances);
  ~FindJSObjectsVisitor() {}

  uint64_t Visit(uint64_t location, uint64_t available);

  uint32_t FoundCount() { return found_count_; }

 private:
  bool IsAHistogramType(v8::HeapObject& heap_object, v8::Error err);

  lldb::SBTarget& target_;
  uint32_t address_byte_size_;
  uint32_t found_count_;

  TypeRecordMap& mapstoinstances_;
};


class LLScan {
 public:
  LLScan(){};

  bool ScanHeapForObjects(lldb::SBTarget target,
                          lldb::SBCommandReturnObject& result);
  bool GenerateMemoryRanges(lldb::SBTarget target,
                            const char* segmentsfilename);

  inline TypeRecordMap& GetMapsToInstances() { return mapstoinstances_; };

 private:
  void ScanMemoryRanges(FindJSObjectsVisitor& v);
  void ClearMemoryRanges();
  void ClearMapsToInstances();

  class MemoryRange {
   public:
    MemoryRange(uint64_t start, uint64_t length)
        : start_(start), length_(length), next_(nullptr){};

    uint64_t start_;
    uint64_t length_;
    MemoryRange* next_;
  };

  lldb::SBTarget target_;
  lldb::SBProcess process_;
  MemoryRange* ranges_ = nullptr;
  TypeRecordMap mapstoinstances_;
};

}  // llnode


#endif  // SRC_LLSCAN_H_
