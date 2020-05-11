#ifndef SRC_LLSCAN_H_
#define SRC_LLSCAN_H_

#include <lldb/API/LLDB.h>
#include <map>
#include <set>
#include <unordered_set>

#include "src/error.h"
#include "src/llnode.h"
#include "src/printer.h"

namespace llnode {

class LLScan;

typedef std::vector<uint64_t> ReferencesVector;
typedef std::unordered_set<uint64_t> ContextVector;

typedef std::map<uint64_t, ReferencesVector*> ReferencesByValueMap;
typedef std::map<std::string, ReferencesVector*> ReferencesByPropertyMap;
typedef std::map<std::string, ReferencesVector*> ReferencesByStringMap;


// New type defining pagination options
// It should be feasible to use it to any commands that output
// a list of information
struct cmd_pagination_t {
  int total_entries = 0;
  int current_page = 0;
  int output_limit = 0;
  std::string command = "";
};

char** ParsePrinterOptions(char** cmd, Printer::PrinterOptions* options);

class FindObjectsCmd : public CommandBase {
 public:
  FindObjectsCmd(LLScan* llscan) : llscan_(llscan) {}
  ~FindObjectsCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  void SimpleOutput(lldb::SBCommandReturnObject& result);
  void DetailedOutput(lldb::SBCommandReturnObject& result);

 private:
  LLScan* llscan_;
};

class FindInstancesCmd : public CommandBase {
 public:
  FindInstancesCmd(LLScan* llscan, bool detailed)
      : llscan_(llscan), detailed_(detailed) {}
  ~FindInstancesCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  LLScan* llscan_;
  bool detailed_;
  cmd_pagination_t pagination_;
};

class NodeInfoCmd : public CommandBase {
 public:
  NodeInfoCmd(LLScan* llscan) : llscan_(llscan) {}
  ~NodeInfoCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

 private:
  LLScan* llscan_;
};

class ScanOptions {
 public:
  // Defines what are we looking for
  enum ScanType { kFieldValue, kPropertyName, kStringValue, kBadOption };

  ScanOptions() : scan_type(ScanType::kFieldValue), recursive_scan(false) {}

  ScanType scan_type;
  bool recursive_scan;
};

class FindReferencesCmd : public CommandBase {
 public:
  FindReferencesCmd(LLScan* llscan) : llscan_(llscan) {}
  ~FindReferencesCmd() override {}

  bool DoExecute(lldb::SBDebugger d, char** cmd,
                 lldb::SBCommandReturnObject& result) override;

  char** ParseScanOptions(char** cmd, ScanOptions* options);

  class ObjectScanner {
   public:
    virtual ~ObjectScanner() {}

    virtual bool AreReferencesLoaded() { return false; };

    virtual ReferencesVector* GetReferences() { return nullptr; };

    virtual void ScanRefs(v8::JSObject& js_obj, Error& err){};
    virtual void ScanRefs(v8::String& str, Error& err){};

    virtual void PrintRefs(lldb::SBCommandReturnObject& result,
                           v8::JSObject& js_obj, Error& err, int level = 0) {}

    virtual void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                           Error& err, int level = 0) {}

    virtual void PrintContextRefs(lldb::SBCommandReturnObject& result,
                                  Error& err, FindReferencesCmd* cli_cmd_,
                                  ScanOptions* options,
                                  ReferencesVector* already_visited_references,
                                  int level = 0) {}

    std::string GetPropertyReferenceString(int level = 0);
    std::string GetArrayReferenceString(int level = 0);
  };

  void PrintReferences(lldb::SBCommandReturnObject& result,
                       ReferencesVector* references, ObjectScanner* scanner,
                       ScanOptions* options,
                       ReferencesVector* already_visited_references,
                       int level = 0);

  void ScanForReferences(ObjectScanner* scanner);

  void PrintRecursiveReferences(lldb::SBCommandReturnObject& result,
                                ScanOptions* options,
                                ReferencesVector* visited_references,
                                uint64_t address, int level);

  class ReferenceScanner : public ObjectScanner {
   public:
    ReferenceScanner(LLScan* llscan, v8::Value search_value)
        : llscan_(llscan), search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, Error& err) override;
    void ScanRefs(v8::String& str, Error& err) override;

    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   Error& err, int level = 0) override;
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                   Error& err, int level = 0) override;

    void PrintContextRefs(lldb::SBCommandReturnObject& result, Error& err,
                          FindReferencesCmd* cli_cmd_, ScanOptions* options,
                          ReferencesVector* already_visited_references,
                          int level = 0) override;

   private:
    LLScan* llscan_;
    v8::Value search_value_;
  };

  class PropertyScanner : public ObjectScanner {
   public:
    PropertyScanner(LLScan* llscan, std::string search_value)
        : llscan_(llscan), search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, Error& err) override;

    // We only scan properties on objects not Strings, use default no-op impl
    // of PrintRefs for Strings.
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   Error& err, int level = 0) override;

   private:
    LLScan* llscan_;
    std::string search_value_;
  };


  class StringScanner : public ObjectScanner {
   public:
    StringScanner(LLScan* llscan, std::string search_value)
        : llscan_(llscan), search_value_(search_value) {}

    bool AreReferencesLoaded() override;

    ReferencesVector* GetReferences() override;

    void ScanRefs(v8::JSObject& js_obj, Error& err) override;
    void ScanRefs(v8::String& str, Error& err) override;

    void PrintRefs(lldb::SBCommandReturnObject& result, v8::JSObject& js_obj,
                   Error& err, int level = 0) override;
    void PrintRefs(lldb::SBCommandReturnObject& result, v8::String& str,
                   Error& err, int level = 0) override;

    static const char* const property_reference_template;
    static const char* const array_reference_template;

   private:
    LLScan* llscan_;
    std::string search_value_;
  };

 private:
  LLScan* llscan_;  // FindReferencesCmd::llscan_
};

class MemoryVisitor {
 public:
  virtual ~MemoryVisitor() {}

  virtual uint64_t Visit(uint64_t location, uint64_t available) = 0;
};

class DetailedTypeRecord;

class TypeRecord {
 public:
  TypeRecord(std::string& type_name)
      : type_name_(type_name), instance_count_(0), total_instance_size_(0) {}

  inline std::string& GetTypeName() { return type_name_; };
  inline uint64_t GetInstanceCount() { return instance_count_; };
  inline uint64_t GetTotalInstanceSize() { return total_instance_size_; };
  inline std::unordered_set<uint64_t>& GetInstances() { return instances_; };

  inline void AddInstance(uint64_t address, uint64_t size) {
    auto result = instances_.insert(address);
    if (result.second) {
      instance_count_++;
      total_instance_size_ += size;
    }
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
  friend class DetailedTypeRecord;
  std::string type_name_;
  uint64_t instance_count_;
  uint64_t total_instance_size_;
  std::unordered_set<uint64_t> instances_;
};

class DetailedTypeRecord : public TypeRecord {
 public:
  DetailedTypeRecord(std::string& type_name, uint64_t own_descriptors_count,
                     uint64_t indexed_properties_count)
      : TypeRecord(type_name),
        own_descriptors_count_(own_descriptors_count),
        indexed_properties_count_(indexed_properties_count) {}
  uint64_t GetOwnDescriptorsCount() const { return own_descriptors_count_; };
  uint64_t GetIndexedPropertiesCount() const {
    return indexed_properties_count_;
  };

 private:
  std::vector<std::string> properties_;
  uint64_t own_descriptors_count_;
  uint64_t indexed_properties_count_;
};

typedef std::map<std::string, TypeRecord*> TypeRecordMap;
typedef std::map<std::string, DetailedTypeRecord*> DetailedTypeRecordMap;

class FindJSObjectsVisitor : MemoryVisitor {
 public:
  FindJSObjectsVisitor(lldb::SBTarget& target, LLScan* llscan);
  ~FindJSObjectsVisitor() {}

  uint64_t Visit(uint64_t location, uint64_t word);

  uint32_t FoundCount() { return found_count_; }

 private:
  // TODO (mmarchini): this could be an option for findjsobjects
  static const size_t kNumberOfPropertiesForDetailedOutput = 3;

  struct MapCacheEntry {
    enum ShowArrayLength { kShowArrayLength, kDontShowArrayLength };

    std::string type_name;
    bool is_histogram;
    bool is_context;

    std::vector<std::string> properties_;
    uint64_t own_descriptors_count_ = 0;
    uint64_t indexed_properties_count_ = 0;

    std::string GetTypeNameWithProperties(
        ShowArrayLength show_array_length = kShowArrayLength,
        size_t max_properties = 0);

    bool Load(v8::Map map, v8::HeapObject heap_object, v8::LLV8* llv8,
              Error& err);
  };

  static bool IsAHistogramType(v8::Map& map, Error& err);

  void InsertOnContexts(uint64_t word, Error& err);
  void InsertOnMapsToInstances(uint64_t word, v8::Map map,
                               FindJSObjectsVisitor::MapCacheEntry map_info,
                               Error& err);
  void InsertOnDetailedMapsToInstances(
      uint64_t word, v8::Map map, FindJSObjectsVisitor::MapCacheEntry map_info,
      Error& err);

  lldb::SBTarget& target_;
  uint32_t address_byte_size_;
  uint32_t found_count_;

  LLScan* const llscan_;
  std::map<int64_t, MapCacheEntry> map_cache_;
};

class SnapshotDataCmd : public CommandBase {
  public:
    struct Node{
      enum Type { /** node type (see HeapGraphNode::Type). in v8-profiler.h in v8 source */
        kHidden = 0,  
        kArray = 1,  
        kString = 2, 
        kObject = 3,  
        kCode = 4,  
        kClosure = 5,  
        kRegExp = 6,  
        kHeapNumber = 7,  
        kNative = 8,  
        kSynthetic = 9,  
        kConsString = 10,  
        kSlicedString = 11,  
        kSymbol = 12,  
        kSimdValue = 13,  
        kInvalid = -1
      };

      Type type;
      uint64_t addr;
      uint64_t name_id;
      uint64_t node_id;
      uint64_t self_size;
      uint64_t children;
      uint64_t trace_node_id;
    };

    struct Edge {
      enum Type {
        kContextVariable = 0, 
        kElement = 1, 
        kProperty = 2, 
        kInternal = 3, 
        kHidden = 4, 
        kShortcut = 5, 
        kWeak = 6 
      };

      Type type;
      uint64_t index_or_property;
      uint64_t to_id;
      uint64_t to_addr;
    };
   
    ~SnapshotDataCmd() override {}

    bool DoExecute(lldb::SBDebugger d, char** cmd, lldb::SBCommandReturnObject& result) override;

    Node::Type GetInstanceType(Error& err, uint64_t word);
    
  private:
    LLScan* llscan_;
    std::vector<Node> nodes_
};

class LLScan {
 public:
  LLScan(v8::LLV8* llv8) : llv8_(llv8) {}

  v8::LLV8* v8() { return llv8_; }

  bool ScanHeapForObjects(lldb::SBTarget target,
                          lldb::SBCommandReturnObject& result);

  inline TypeRecordMap& GetMapsToInstances() { return mapstoinstances_; };
  inline DetailedTypeRecordMap& GetDetailedMapsToInstances() {
    return detailedmapstoinstances_;
  };

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

  // References By Property
  inline bool AreReferencesByPropertyLoaded() {
    return references_by_property_.size() > 0;
  };
  inline ReferencesVector* GetReferencesByProperty(std::string property) {
    if (references_by_property_.count(property) == 0) {
      references_by_property_[property] = new ReferencesVector;
    }
    return references_by_property_[property];
  };

  // References By String
  inline bool AreReferencesByStringLoaded() {
    return references_by_string_.size() > 0;
  };
  inline ReferencesVector* GetReferencesByString(std::string string_value) {
    if (references_by_string_.count(string_value) == 0) {
      references_by_string_[string_value] = new ReferencesVector;
    }
    return references_by_string_[string_value];
  };

  // Contexts
  inline bool AreContextsLoaded() { return contexts_.size() > 0; };
  inline ContextVector* GetContexts() { return &contexts_; }

  v8::LLV8* llv8_;

 private:
  void ScanMemoryRegions(FindJSObjectsVisitor& v);
  void ClearMapsToInstances();
  void ClearReferences();

  lldb::SBTarget target_;
  lldb::SBProcess process_;
  TypeRecordMap mapstoinstances_;
  DetailedTypeRecordMap detailedmapstoinstances_;

  ReferencesByValueMap references_by_value_;
  ReferencesByPropertyMap references_by_property_;
  ReferencesByStringMap references_by_string_;
  ContextVector contexts_;
};

}  // namespace llnode


#endif  // SRC_LLSCAN_H_
