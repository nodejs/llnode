#ifndef SRC_LLSCAN_H_
#define SRC_LLSCAN_H_

#include <lldb/API/LLDB.h>
#include <map>
#include <set>
#include "src/llnode.h"

namespace llnode {

typedef std::vector<uint64_t> ReferencesVector;

typedef std::map<uint64_t, ReferencesVector*> ReferencesByValueMap;
typedef std::map<std::string, ReferencesVector*> ReferencesByPropertyMap;
typedef std::map<std::string, ReferencesVector*> ReferencesByStringMap;


class FindObjectsCmd : public CommandBase {
 public:
  ~FindObjectsCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class FindInstancesCmd : public CommandBase {
 public:
  ~FindInstancesCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  bool detailed_;
};

class NodeInfoCmd : public CommandBase {
 public:
  ~NodeInfoCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;
};

class FindReferencesCmd : public CommandBase {
 public:
  ~FindReferencesCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  enum ScanType { kFieldValue, kPropertyName, kStringValue, kBadOption };

  char** ParseScanOptions(char** cmd, ScanType* type);

  class ObjectScanner {
   public:
    virtual ~ObjectScanner() {}

    virtual bool AreReferencesLoaded(){};

    virtual ReferencesVector* GetReferences(){};

    virtual void ScanRefs(v8::JSObject& js_obj, v8::Error& err){};
    virtual void ScanRefs(v8::String& str, v8::Error& err){};

    virtual void PrintRefs(lldb::SBCommandReturnObject& result,
                           v8::JSObject& js_obj, v8::Error& err) {}
    virtual void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                           v8::Error& err) {}
  };

  void PrintReferences(lldb::SBCommandReturnObject& result,
                       ReferencesVector* references, ObjectScanner* scanner);

  void ScanForReferences(ObjectScanner* scanner);

  class ReferenceScanner : public ObjectScanner {
   public:
    ReferenceScanner(v8::Value search_value) : search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, v8::Error& err) override;
    void ScanRefs(v8::String& str, v8::Error& err) override;

    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   v8::Error& err) override;
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                   v8::Error& err) override;

   private:
    v8::Value search_value_;
  };

  class PropertyScanner : public ObjectScanner {
   public:
    PropertyScanner(std::string search_value) : search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, v8::Error& err) override;

    // We only scan properties on objects not Strings, use default no-op impl
    // of PrintRefs for Strings.
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   v8::Error& err) override;

   private:
    std::string search_value_;
  };


  class StringScanner : public ObjectScanner {
   public:
    StringScanner(std::string search_value) : search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, v8::Error& err) override;
    void ScanRefs(v8::String& str, v8::Error& err) override;

    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   v8::Error& err) override;
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                   v8::Error& err) override;

   private:
    std::string search_value_;
  };
};

class MemoryVisitor {
 public:
  virtual ~MemoryVisitor() {}

  virtual uint64_t Visit(uint64_t location, uint64_t available) = 0;
};

class TypeRecord {
 public:
  TypeRecord(std::string& type_name)
      : type_name_(type_name), instance_count_(0), total_instance_size_(0) {}

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

  uint64_t Visit(uint64_t location, uint64_t word);

  uint32_t FoundCount() { return found_count_; }

 private:
  struct MapCacheEntry {
    std::string type_name;
    bool is_histogram;
  };

  bool IsAHistogramType(v8::Map& map, v8::Error& err);

  lldb::SBTarget& target_;
  uint32_t address_byte_size_;
  uint32_t found_count_;

  TypeRecordMap& mapstoinstances_;
  std::map<int64_t, MapCacheEntry> map_cache_;
};


class LLScan {
 public:
  LLScan() {}

  bool ScanHeapForObjects(lldb::SBTarget target,
                          lldb::SBCommandReturnObject& result);
  bool GenerateMemoryRanges(lldb::SBTarget target,
                            const char* segmentsfilename);

  inline TypeRecordMap& GetMapsToInstances() { return mapstoinstances_; };

  // References By Value
  inline bool AreReferencesByValueLoaded() {
    return references_by_value_.size() > 0;
  };
  inline ReferencesVector* GetReferencesByValue(uint64_t address) {
    if (references_by_value_.count(address) == 0) {
      references_by_value_[address] = new ReferencesVector;
    }
    return references_by_value_[address];
  };

  inline bool AreReferencesByPropertyLoaded() {
    return references_by_property_.size() > 0;
  };
  inline ReferencesVector* GetReferencesByProperty(std::string property) {
    if (references_by_property_.count(property) == 0) {
      references_by_property_[property] = new ReferencesVector;
    }
    return references_by_property_[property];
  };

  inline bool AreReferencesByStringLoaded() {
    return references_by_string_.size() > 0;
  };
  inline ReferencesVector* GetReferencesByString(std::string string_value) {
    if (references_by_string_.count(string_value) == 0) {
      references_by_string_[string_value] = new ReferencesVector;
    }
    return references_by_string_[string_value];
  };

 private:
  void ScanMemoryRanges(FindJSObjectsVisitor& v);
  void ClearMemoryRanges();
  void ClearMapsToInstances();

  class MemoryRange {
   public:
    MemoryRange(uint64_t start, uint64_t length)
        : start_(start), length_(length), next_(nullptr) {}

    uint64_t start_;
    uint64_t length_;
    MemoryRange* next_;
  };

  lldb::SBTarget target_;
  lldb::SBProcess process_;
  MemoryRange* ranges_ = nullptr;
  TypeRecordMap mapstoinstances_;

  ReferencesByValueMap references_by_value_;
  ReferencesByPropertyMap references_by_property_;
  ReferencesByStringMap references_by_string_;
};

}  // namespace llnode


#endif  // SRC_LLSCAN_H_
