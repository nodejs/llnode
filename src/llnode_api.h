// C++ wrapper API for lldb and llnode APIs

#ifndef SRC_LLNODE_API_H_
#define SRC_LLNODE_API_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace lldb {
class SBDebugger;
class SBTarget;
class SBProcess;

}  // namespace lldb

namespace llnode {

class LLScan;
class TypeRecord;

namespace v8 {
class LLV8;
}

class LLNodeApi {
 public:
  // TODO: a status class

  LLNodeApi();
  ~LLNodeApi();
  LLNodeApi(LLNodeApi&&);
  LLNodeApi& operator=(LLNodeApi&&);

  bool Init(const char* filename, const char* executable);
  std::string GetProcessInfo();
  uint32_t GetProcessID();
  std::string GetProcessState();
  uint32_t GetThreadCount();
  uint32_t GetFrameCount(size_t thread_index);
  std::string GetFrame(size_t thread_index, size_t frame_index);
  void ScanHeap();
  // Must be called after ScanHeap;
  uint32_t GetTypeCount();
  std::string GetTypeName(size_t type_index);
  uint32_t GetTypeInstanceCount(size_t type_index);
  uint32_t GetTypeTotalSize(size_t type_index);
  std::set<uint64_t>* GetTypeInstances(size_t type_index);
  std::string GetObject(uint64_t address);

 private:
  bool loaded;
  static bool debugger_initialized_;
  std::unique_ptr<lldb::SBDebugger> debugger;
  std::unique_ptr<lldb::SBTarget> target;
  std::unique_ptr<lldb::SBProcess> process;
  std::unique_ptr<v8::LLV8> llv8;
  std::unique_ptr<LLScan> llscan;
  std::vector<TypeRecord*> object_types;
};

}  // namespace llnode

#endif  // SRC_LLNODE_API_H_
