#ifndef SRC_LLV8_H_
#define SRC_LLV8_H_

#include <string>

#include <lldb/API/LLDB.h>

namespace llnode {

class LLV8 {
 public:
  LLV8(lldb::SBTarget target);

  bool GetJSFrameName(lldb::addr_t frame, std::string& out);
  bool GetJSFunctionName(lldb::addr_t fn, std::string& out);
  bool GetSharedInfoPostfix(lldb::addr_t addr, std::string& out);
  bool ToString(lldb::addr_t value, std::string& out);

 private:
  bool is_smi(int64_t value) const;
  int64_t untag_smi(int64_t value) const;
  bool is_heap_obj(int64_t value) const;

  inline bool LoadPtr(lldb::addr_t addr, int64_t off, int64_t* out);
  inline int64_t LeaHeapField(int64_t addr, int64_t off);
  inline bool LoadHeapField(int64_t addr, int64_t off, int64_t* out);
  inline bool GetMap(int64_t addr, int64_t* map);

  int64_t LoadConstant(const char* name);
  bool LoadPtr(int64_t addr, int64_t* out);
  bool LoadString(int64_t addr, int64_t length, std::string& out);
  bool LoadTwoByteString(int64_t addr, int64_t length, std::string& out);

  bool GetType(int64_t addr, int64_t* out);

  lldb::SBTarget target_;
  lldb::SBProcess process_;

  struct {
    int64_t kTag;
    int64_t kTagMask;
    int64_t kShiftSize;
  } smi_;

  struct {
    int64_t kTag;
    int64_t kTagMask;

    int64_t kMapOffset;
  } heap_obj_;

  struct {
    int64_t kInstanceAttrsOffset;
  } map_;

  struct {
    int64_t kSharedInfoOffset;
  } js_function_;

  struct {
    int64_t kNameOffset;
    int64_t kInferredNameOffset;
    int64_t kScriptOffset;
  } shared_info_;

  struct {
    int64_t kNameOffset;
    int64_t kLineOffset;
  } script_;

  struct {
    int64_t kEncodingMask;
    int64_t kRepresentationMask;

    // Encoding
    int64_t kOneByteStringTag;
    int64_t kTwoByteStringTag;

    // Representation
    int64_t kSeqStringTag;
    int64_t kConsStringTag;

    int64_t kLengthOffset;
  } string_;

  struct {
    int64_t kCharsOffset;
  } one_byte_string_;

  struct {
    int64_t kCharsOffset;
  } two_byte_string_;

  struct {
    int64_t kFirstOffset;
    int64_t kSecondOffset;
  } cons_string_;

  struct {
    int64_t kContextOffset;
    int64_t kFunctionOffset;
    int64_t kArgsOffset;
    int64_t kMarkerOffset;

    int64_t kAdaptorFrame;
    int64_t kEntryFrame;
    int64_t kEntryConstructFrame;
    int64_t kExitFrame;
    int64_t kInternalFrame;
    int64_t kConstructFrame;
    int64_t kJSFrame;
    int64_t kOptimizedFrame;
  } frame_;

  struct {
    int64_t kFirstNonstringType;

    int64_t kCodeType;
    int64_t kJSFunctionType;
  } types_;
};

}  // namespace llnode

#endif  // SRC_LLV8_H_
