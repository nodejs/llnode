#include <string.h>

#include "src/llv8-code-map.h"
#include "src/llv8-inl.h"

namespace llnode {
namespace v8 {

std::string CodeMap::Collect(Error& err) {
  int64_t isolate = v8()->node()->kNodeIsolate;
  if (isolate <= 0) {
    err = Error::Failure("No isolate found");
    return std::string();
  }

  int64_t heap = isolate + v8()->node()->kIsolateHeapOffset;
  int64_t old_space = FindOldSpace(heap, err);
  if (err.Fail()) return std::string();
  int64_t anchor = old_space + v8()->node()->kOldSpaceAnchorOffset;

  std::string res;

  int64_t current = anchor;
  do {
    current = v8()->LoadPtr(current + v8()->node()->kPageNextOffset, err);
    if (err.Fail()) break;
    if (current == anchor) break;

    int64_t area_start =
        v8()->LoadPtr(current + v8()->node()->kPageAreaStartOffset, err);
    if (err.Fail()) break;

    int64_t area_end =
        v8()->LoadPtr(current + v8()->node()->kPageAreaEndOffset, err);
    if (err.Fail()) break;

    res += CollectArea(area_start, area_end, err);
    if (err.Fail()) continue;
  } while (current != anchor);

  err = Error::Ok();
  return res;
}


int64_t CodeMap::FindOldSpace(int64_t heap, Error& err) {
  int64_t kPointerSize = v8()->common()->kPointerSize;

  for (int64_t off = 0; off < kMaxOldSpaceSearch; off += kPointerSize) {
    int64_t probe = v8()->LoadPtr(heap + off, err);
    if (err.Fail()) continue;

    // Skip global roots
    HeapObject heap_obj(v8(), probe);
    if (heap_obj.Check()) continue;

    // space->heap_ == heap
    int64_t tmp = v8()->LoadPtr(probe + v8()->node()->kOldSpaceHeapOffset, err);
    if (err.Fail()) continue;
    if (tmp != heap) continue;

    // space->id_ == OLD_SPACE
    tmp = v8()->LoadPtr(probe + v8()->node()->kOldSpaceIdOffset, err);
    tmp &= 0xffffffff;
    if (err.Fail()) continue;
    if (tmp != v8()->node()->kOldSpaceId) continue;

    // !space->executable_
    tmp = v8()->LoadPtr(probe + v8()->node()->kOldSpaceExecutableOffset, err);
    tmp &= 0xffffffff;
    if (err.Fail()) continue;
    if (tmp != 0) continue;

    // Check that pages are linked to each other
    int64_t start = probe + v8()->node()->kOldSpaceAnchorOffset;
    int64_t page = start;
    int counter = 0;
    do {
      page = v8()->LoadPtr(page + v8()->node()->kPageNextOffset, err);
      if (err.Fail()) break;
      counter++;
    } while (page != start && counter < kMaxPageCheck);

    if (page != start) continue;

    return probe;
  }

  err = Error::Failure("OldSpace not found");
  return -1;
}


std::string CodeMap::CollectArea(int64_t start, int64_t end, Error& err) {
  int64_t kPointerSize = v8()->common()->kPointerSize;

  std::string res;
  for (int64_t current = start; current < end; current += kPointerSize) {
    int64_t ptr = v8()->LoadPtr(current, err);
    if (err.Fail()) break;

    Value val(v8(), ptr);

    Smi smi(val);
    if (smi.Check()) continue;

    HeapObject obj(val);
    if (!obj.Check()) continue;

    // TODO(indutny): use map instance_size to speed up search
    int64_t instance_size;

    res += CollectObject(obj, &instance_size, err);
    if (err.Fail()) continue;

    if (instance_size < kPointerSize)
      instance_size = kPointerSize;
    current += instance_size;
  }

  return res;
}


std::string CodeMap::CollectObject(HeapObject obj, int64_t* size, Error& err) {
  HeapObject map_obj = obj.GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);
  int64_t type = map.GetType(err);
  if (err.Fail()) return std::string();

  *size = map.InstanceSize(err);
  if (err.Fail()) return std::string();

  if (type != v8()->types()->kSharedFunctionInfoType) return std::string();

  SharedFunctionInfo info(obj);
  Code code = info.GetCode(err);
  if (err.Fail()) return std::string();

  type = code.GetType(err);
  if (err.Fail()) return std::string();
  if (type != v8()->types()->kCodeType) return std::string();

  int64_t code_start = code.Start();
  int64_t code_size = code.Size(err);
  if (err.Fail()) return std::string();

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "0x%016llx, 0x%016llx, ", code_start,
      code_start + code_size);
  std::string res = tmp;

  res += info.ToString(err) + "\n";
  if (err.Fail()) return std::string();

  return res;
}

}  // namespace v8
}  // namespace llnode
