#ifndef SRC_LLV8_CONSTANTS_H_
#define SRC_LLV8_CONSTANTS_H_

#include <lldb/API/LLDB.h>

namespace llnode {
namespace v8 {
namespace constants {

// Forward declarations
class Common;

class Module {
 public:
  Module() : loaded_(false) {}

  inline bool is_loaded() const { return loaded_; }

  void Assign(lldb::SBTarget target, Common* common = nullptr);

 protected:
  int64_t LoadRawConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, int64_t def = -1);
  int64_t LoadConstant(const char* name, const char* fallback,
                       int64_t def = -1);

  lldb::SBTarget target_;
  Common* common_;
  bool loaded_;
};

#define MODULE_DEFAULT_METHODS(NAME) \
  NAME() {}                          \
  inline NAME* operator()() {        \
    if (loaded_) return this;        \
    loaded_ = true;                  \
    Load();                          \
    return this;                     \
  }

class Common : public Module {
 public:
  MODULE_DEFAULT_METHODS(Common);

  int64_t kPointerSize;
  int64_t kVersionMajor;
  int64_t kVersionMinor;
  int64_t kVersionPatch;

  bool CheckLowestVersion(int64_t major, int64_t minor, int64_t patch);
  bool CheckHighestVersion(int64_t major, int64_t minor, int64_t patch);

  // Public, because other modules may use it
  void Load();
};

class Smi : public Module {
 public:
  MODULE_DEFAULT_METHODS(Smi);

  int64_t kTag;
  int64_t kTagMask;
  int64_t kShiftSize;

 protected:
  void Load();
};

class HeapObject : public Module {
 public:
  MODULE_DEFAULT_METHODS(HeapObject);

  int64_t kTag;
  int64_t kTagMask;

  int64_t kMapOffset;

 protected:
  void Load();
};

class Map : public Module {
 public:
  MODULE_DEFAULT_METHODS(Map);

  int64_t kInstanceAttrsOffset;
  int64_t kMaybeConstructorOffset;
  int64_t kInstanceDescriptorsOffset;
  int64_t kBitField3Offset;
  int64_t kInObjectPropertiesOffset;
  int64_t kInstanceSizeOffset;

  int64_t kNumberOfOwnDescriptorsMask;
  int64_t kNumberOfOwnDescriptorsShift;
  int64_t kDictionaryMapShift;

 protected:
  void Load();
};

class JSObject : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSObject);

  int64_t kPropertiesOffset;
  int64_t kElementsOffset;
  int64_t kInternalFieldsOffset;

 protected:
  void Load();
};

class HeapNumber : public Module {
 public:
  MODULE_DEFAULT_METHODS(HeapNumber);

  int64_t kValueOffset;

 protected:
  void Load();
};

class JSArray : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSArray);

  int64_t kLengthOffset;

 protected:
  void Load();
};

class JSFunction : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSFunction);

  int64_t kSharedInfoOffset;
  int64_t kContextOffset;

 protected:
  void Load();
};

class JSRegExp : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSRegExp);

  int64_t kSourceOffset;

 protected:
  void Load();
};

class JSDate : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSDate);

  int64_t kValueOffset;

 protected:
  void Load();
};

class SharedInfo : public Module {
 public:
  MODULE_DEFAULT_METHODS(SharedInfo);

  int64_t kNameOffset;
  int64_t kInferredNameOffset;
  int64_t kScriptOffset;
  int64_t kCodeOffset;
  int64_t kStartPositionOffset;
  int64_t kEndPositionOffset;
  int64_t kParameterCountOffset;
  int64_t kScopeInfoOffset;

  int64_t kStartPositionMask;
  int64_t kStartPositionShift;
  int64_t kEndPositionShift;

 protected:
  void Load();
};

class Code : public Module {
 public:
  MODULE_DEFAULT_METHODS(Code)

  int64_t kStartOffset;
  int64_t kSizeOffset;

 protected:
  void Load();
};

class ScopeInfo : public Module {
 public:
  MODULE_DEFAULT_METHODS(ScopeInfo);

  int64_t kParameterCountOffset;
  int64_t kStackLocalCountOffset;
  int64_t kContextLocalCountOffset;
  int64_t kVariablePartIndex;

 protected:
  void Load();
};

class Context : public Module {
 public:
  MODULE_DEFAULT_METHODS(Context);

  int64_t kClosureIndex;
  int64_t kGlobalObjectIndex;
  int64_t kPreviousIndex;
  int64_t kMinContextSlots;

 protected:
  void Load();
};

class Script : public Module {
 public:
  MODULE_DEFAULT_METHODS(Script);

  int64_t kNameOffset;
  int64_t kLineOffsetOffset;
  int64_t kSourceOffset;
  int64_t kLineEndsOffset;

 protected:
  void Load();
};

class String : public Module {
 public:
  MODULE_DEFAULT_METHODS(String);

  int64_t kEncodingMask;
  int64_t kRepresentationMask;

  // Encoding
  int64_t kOneByteStringTag;
  int64_t kTwoByteStringTag;

  // Representation
  int64_t kSeqStringTag;
  int64_t kConsStringTag;
  int64_t kSlicedStringTag;
  int64_t kExternalStringTag;
  int64_t kThinStringTag;

  int64_t kLengthOffset;

 protected:
  void Load();
};

class OneByteString : public Module {
 public:
  MODULE_DEFAULT_METHODS(OneByteString);

  int64_t kCharsOffset;

 protected:
  void Load();
};

class TwoByteString : public Module {
 public:
  MODULE_DEFAULT_METHODS(TwoByteString);

  int64_t kCharsOffset;

 protected:
  void Load();
};

class ConsString : public Module {
 public:
  MODULE_DEFAULT_METHODS(ConsString);

  int64_t kFirstOffset;
  int64_t kSecondOffset;

 protected:
  void Load();
};

class SlicedString : public Module {
 public:
  MODULE_DEFAULT_METHODS(SlicedString);

  int64_t kParentOffset;
  int64_t kOffsetOffset;

 protected:
  void Load();
};

class ThinString : public Module {
 public:
  MODULE_DEFAULT_METHODS(ThinString);

  int64_t kActualOffset;

 protected:
  void Load();
};

class FixedArrayBase : public Module {
 public:
  MODULE_DEFAULT_METHODS(FixedArrayBase);

  int64_t kLengthOffset;

 protected:
  void Load();
};

class FixedArray : public Module {
 public:
  MODULE_DEFAULT_METHODS(FixedArray);

  int64_t kDataOffset;

 protected:
  void Load();
};

class FixedTypedArrayBase : public Module {
 public:
  MODULE_DEFAULT_METHODS(FixedTypedArrayBase);

  int64_t kBasePointerOffset;
  int64_t kExternalPointerOffset;

 protected:
  void Load();
};

class Oddball : public Module {
 public:
  MODULE_DEFAULT_METHODS(Oddball);

  int64_t kKindOffset;

  int64_t kException;
  int64_t kFalse;
  int64_t kTrue;
  int64_t kUndefined;
  int64_t kNull;
  int64_t kTheHole;
  int64_t kUninitialized;

 protected:
  void Load();
};

class JSArrayBuffer : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSArrayBuffer);

  int64_t kKindOffset;

  int64_t kBackingStoreOffset;
  int64_t kByteLengthOffset;
  int64_t kBitFieldOffset;

  int64_t kWasNeuteredMask;
  int64_t kWasNeuteredShift;

 protected:
  void Load();
};

class JSArrayBufferView : public Module {
 public:
  MODULE_DEFAULT_METHODS(JSArrayBufferView);

  int64_t kBufferOffset;
  int64_t kByteOffsetOffset;
  int64_t kByteLengthOffset;

 protected:
  void Load();
};

class DescriptorArray : public Module {
 public:
  MODULE_DEFAULT_METHODS(DescriptorArray);

  int64_t kDetailsOffset;
  int64_t kKeyOffset;
  int64_t kValueOffset;

  int64_t kPropertyIndexMask;
  int64_t kPropertyIndexShift;
  int64_t kRepresentationMask;
  int64_t kRepresentationShift;

  int64_t kRepresentationDouble;

  int64_t kFirstIndex;
  int64_t kSize;

  // node.js <= 7
  int64_t kPropertyTypeMask = -1;
  int64_t kConstFieldType = -1;
  int64_t kFieldType = -1;

  // node.js >= 8
  int64_t kPropertyAttributesMask = -1;
  int64_t kPropertyAttributesShift = -1;
  int64_t kPropertyAttributesEnum_NONE = -1;
  int64_t kPropertyAttributesEnum_READ_ONLY = -1;
  int64_t kPropertyAttributesEnum_DONT_ENUM = -1;
  int64_t kPropertyAttributesEnum_DONT_DELETE = -1;

  int64_t kPropertyKindMask = -1;
  int64_t kPropertyKindShift = -1;
  int64_t kPropertyKindEnum_kAccessor = -1;
  int64_t kPropertyKindEnum_kData = -1;

  int64_t kPropertyLocationMask = -1;
  int64_t kPropertyLocationShift = -1;
  int64_t kPropertyLocationEnum_kDescriptor = -1;
  int64_t kPropertyLocationEnum_kField = -1;

 protected:
  void Load();
};

class NameDictionary : public Module {
 public:
  MODULE_DEFAULT_METHODS(NameDictionary);

  int64_t kKeyOffset;
  int64_t kValueOffset;

  int64_t kEntrySize;
  int64_t kPrefixStartIndex;
  int64_t kPrefixSize;

 protected:
  void Load();
};

class Frame : public Module {
 public:
  MODULE_DEFAULT_METHODS(Frame);

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
  int64_t kStubFrame;

 protected:
  void Load();
};

class Types : public Module {
 public:
  MODULE_DEFAULT_METHODS(Types);

  int64_t kFirstNonstringType;

  int64_t kHeapNumberType;
  int64_t kMapType;
  int64_t kGlobalObjectType;
  int64_t kGlobalProxyType;
  int64_t kOddballType;
  int64_t kJSObjectType;
  int64_t kJSAPIObjectType;
  int64_t kJSSpecialAPIObjectType;
  int64_t kJSArrayType;
  int64_t kCodeType;
  int64_t kJSFunctionType;
  int64_t kFixedArrayType;
  int64_t kJSArrayBufferType;
  int64_t kJSTypedArrayType;
  int64_t kJSRegExpType;
  int64_t kJSDateType;
  int64_t kSharedFunctionInfoType;
  int64_t kScriptType;

 protected:
  void Load();
};

#undef MODULE_DEFAULT_METHODS

}  // namespace constants
}  // namespace v8
}  // namespace llnode

#endif  // SRC_LLV8_CONSTANTS_H_
