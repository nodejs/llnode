#include <string>

#include "src/llv8-constants.h"

namespace llnode {
namespace v8 {
namespace constants {

using namespace lldb;

static std::string kConstantPrefix = "v8dbg_";

void Module::Assign(SBTarget target, Common* common) {
  loaded_ = false;
  target_ = target;
  common_ = common;
}


int64_t Module::LoadRawConstant(const char* name, int64_t def) {
  SBValue v = target_.FindFirstGlobalVariable(name);

  if (v.GetError().Fail())
    fprintf(stderr, "Failed to load %s\n", name);

  return v.GetValueAsSigned(def);
}


int64_t Module::LoadConstant(const char* name, int64_t def) {
  SBValue v = target_.FindFirstGlobalVariable((kConstantPrefix + name).c_str());

  if (v.GetError().Fail())
    fprintf(stderr, "Failed to load %s\n", name);

  return v.GetValueAsSigned(def);
}


int64_t Module::LoadConstant(const char* name, const char* fallback,
                             int64_t def) {
  SBValue v = target_.FindFirstGlobalVariable((kConstantPrefix + name).c_str());

  if (v.GetError().Fail())
    v = target_.FindFirstGlobalVariable((kConstantPrefix + fallback).c_str());

  if (v.GetError().Fail())
    fprintf(stderr, "Failed to load both %s and %s\n", name, fallback);

  return v.GetValueAsSigned(def);
}


void Common::Load() {
  kPointerSize = 1 << LoadConstant("PointerSizeLog2");
  kVersionMajor = LoadRawConstant("v8::internal::Version::major_");
  kVersionMinor = LoadRawConstant("v8::internal::Version::minor_");
}


void Smi::Load() {
  kTag = LoadConstant("SmiTag");
  kTagMask = LoadConstant("SmiTagMask");
  kShiftSize = LoadConstant("SmiShiftSize");
}


void HeapObject::Load() {
  kTag = LoadConstant("HeapObjectTag");
  kTagMask = LoadConstant("HeapObjectTagMask");
  kMapOffset = LoadConstant("class_HeapObject__map__Map");
}


void Map::Load() {
  kInstanceAttrsOffset =
      LoadConstant("class_Map__instance_attributes__int");
  kMaybeConstructorOffset =
      LoadConstant("class_Map__constructor_or_backpointer__Object",
                   "class_Map__constructor__Object");
  kInstanceDescriptorsOffset =
      LoadConstant("class_Map__instance_descriptors__DescriptorArray");
  kBitField3Offset = LoadConstant("class_Map__bit_field3__int",
                                  "class_Map__bit_field3__SMI");
  kInObjectPropertiesOffset = LoadConstant(
      "class_Map__inobject_properties_or_constructor_function_index__int",
      "class_Map__inobject_properties__int");
  kInstanceSizeOffset = LoadConstant("class_Map__instance_size__int");

  kDictionaryMapShift = LoadConstant("bit_field3_dictionary_map_shift");

  // TODO(indutny): PR for more postmortem data
  static const int64_t kDescriptorIndexBitCount = 10;
  kNumberOfOwnDescriptorsShift = kDictionaryMapShift - kDescriptorIndexBitCount;
  kNumberOfOwnDescriptorsMask = (1 << kDescriptorIndexBitCount) - 1;
  kNumberOfOwnDescriptorsMask <<= kNumberOfOwnDescriptorsShift;
}


void JSObject::Load() {
  kPropertiesOffset = LoadConstant("class_JSObject__properties__FixedArray");
  kElementsOffset = LoadConstant("class_JSObject__elements__Object");
}


void HeapNumber::Load() {
  kValueOffset = LoadConstant("class_HeapNumber__value__double");
}


void JSArray::Load() {
  kLengthOffset = LoadConstant("class_JSArray__length__Object");
}


void JSFunction::Load() {
  kSharedInfoOffset =
      LoadConstant("class_JSFunction__shared__SharedFunctionInfo");
}


void SharedInfo::Load() {
  kNameOffset = LoadConstant("class_SharedFunctionInfo__name__Object");
  kInferredNameOffset =
      LoadConstant("class_SharedFunctionInfo__inferred_name__String");
  kScriptOffset = LoadConstant("class_SharedFunctionInfo__script__Object");
  kStartPositionOffset =
      LoadConstant("class_SharedFunctionInfo__start_position_and_type__SMI");
  kParameterCountOffset = LoadConstant(
      "class_SharedFunctionInfo__internal_formal_parameter_count__SMI",
      "class_SharedFunctionInfo__formal_parameter_count__SMI");

  // TODO(indutny): move it to post-mortem
  kStartPositionShift = 2;
}


void Script::Load() {
  kNameOffset = LoadConstant("class_Script__name__Object");
  kLineOffsetOffset = LoadConstant("class_Script__line_offset__SMI");
  kSourceOffset = LoadConstant("class_Script__source__Object");
  kLineEndsOffset = LoadConstant("class_Script__line_ends__Object");
}


void String::Load() {
  kEncodingMask = LoadConstant("StringEncodingMask");
  kRepresentationMask = LoadConstant("StringRepresentationMask");

  kOneByteStringTag = LoadConstant("OneByteStringTag", "AsciiStringTag");
  kTwoByteStringTag = LoadConstant("TwoByteStringTag");
  kSeqStringTag = LoadConstant("SeqStringTag");
  kConsStringTag = LoadConstant("ConsStringTag");
  kSlicedStringTag = LoadConstant("SlicedStringTag");
  kExternalStringTag = LoadConstant("ExternalStringTag");

  kLengthOffset = LoadConstant("class_String__length__SMI");
}


void OneByteString::Load() {
  kCharsOffset = LoadConstant("class_SeqOneByteString__chars__char",
      "class_SeqAsciiString__chars__char");
}


void TwoByteString::Load() {
  kCharsOffset = LoadConstant("class_SeqTwoByteString__chars__char",
      "class_SeqAsciiString__chars__char");
}


void ConsString::Load() {
  kFirstOffset = LoadConstant("class_ConsString__first__String");
  kSecondOffset = LoadConstant("class_ConsString__second__String");
}


void SlicedString::Load() {
  kParentOffset = LoadConstant("class_SlicedString__parent__String");
  kOffsetOffset = LoadConstant("class_SlicedString__offset__SMI");
}


void FixedArrayBase::Load() {
  kLengthOffset = LoadConstant("class_FixedArrayBase__length__SMI");
}


void FixedArray::Load() {
  kDataOffset = LoadConstant("class_FixedArray__data__uintptr_t");
}


void Oddball::Load() {
  kKindOffset = LoadConstant("class_Oddball__kind_offset__int");

  kException = LoadConstant("OddballException");
  kFalse = LoadConstant("OddballFalse");
  kTrue = LoadConstant("OddballTrue");
  kUndefined = LoadConstant("OddballUndefined");
  kTheHole = LoadConstant("OddballTheHole");
  kNull = LoadConstant("OddballNull");
  kUninitialized = LoadConstant("OddballUninitialized");
}


void JSArrayBuffer::Load() {
  kBackingStoreOffset =
      LoadConstant("class_JSArrayBuffer__backing_store__Object");
  kByteLengthOffset = LoadConstant("class_JSArrayBuffer__byte_length__Object");

  // v4 compatibility fix
  if (kBackingStoreOffset == 0) {
    common_->Load();

    kBackingStoreOffset = kByteLengthOffset + common_->kPointerSize;
    kBitFieldOffset = kBackingStoreOffset + common_->kPointerSize;
    if (common_->kPointerSize == 8)
      kBitFieldOffset += 4;
  }

  // TODO(indutny): move to postmortem info
  kWasNeuteredMask = 1 << 3;
}


void JSArrayBufferView::Load() {
  kBufferOffset = LoadConstant("class_JSArrayBufferView__buffer__Object");
  kByteOffsetOffset =
      LoadConstant("class_JSArrayBufferView__raw_byte_offset__Object");
  kByteLengthOffset =
      LoadConstant("class_JSArrayBufferView__raw_byte_length__Object");
}


void DescriptorArray::Load() {
  kDetailsOffset = LoadConstant("prop_desc_details");
  kKeyOffset = LoadConstant("prop_desc_key");
  kValueOffset = LoadConstant("prop_desc_value");

  kPropertyIndexMask = LoadConstant("prop_index_mask");
  kPropertyIndexShift = LoadConstant("prop_index_shift");
  kPropertyTypeMask = LoadConstant("prop_type_mask");

  // TODO(indutny): move to postmortem
  kRepresentationShift = 5;
  kRepresentationMask = ((1 << 4) - 1) << kRepresentationShift;

  kFieldType = LoadConstant("prop_type_field");
  // TODO(indutny): move to postmortem
  kRepresentationDouble = 7;

  kFirstIndex = LoadConstant("prop_idx_first");
  kSize = LoadConstant("prop_desc_size");
}


void NameDictionary::Load() {
  // TODO(indutny): move this to postmortem
  kKeyOffset = 0;
  kValueOffset = 1;

  kEntrySize = LoadConstant("class_NameDictionaryShape__entry_size__int");

  // TODO(indutny): move extra entry size bytes to postmortem
  kPrefixSize = LoadConstant("class_NameDictionaryShape__prefix_size__int") +
      kEntrySize;
}


void Frame::Load() {
  kContextOffset = LoadConstant("off_fp_context");
  kFunctionOffset = LoadConstant("off_fp_function");
  kArgsOffset = LoadConstant("off_fp_args");
  kMarkerOffset = LoadConstant("off_fp_marker");

  kAdaptorFrame = LoadConstant("frametype_ArgumentsAdaptorFrame");
  kEntryFrame = LoadConstant("frametype_EntryFrame");
  kEntryConstructFrame = LoadConstant("frametype_EntryConstructFrame");
  kExitFrame = LoadConstant("frametype_ExitFrame");
  kInternalFrame = LoadConstant("frametype_InternalFrame");
  kConstructFrame = LoadConstant("frametype_ConstructFrame");
  kJSFrame = LoadConstant("frametype_JavaScriptFrame");
  kOptimizedFrame = LoadConstant("frametype_OptimizedFrame");
}


void Types::Load() {
  kFirstNonstringType = LoadConstant("FirstNonstringType");

  kHeapNumberType = LoadConstant("type_HeapNumber__HEAP_NUMBER_TYPE");
  kMapType = LoadConstant("type_Map__MAP_TYPE");
  kGlobalObjectType =
      LoadConstant("type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE");
  kOddballType = LoadConstant("type_Oddball__ODDBALL_TYPE");
  kJSObjectType = LoadConstant("type_JSObject__JS_OBJECT_TYPE");
  kJSArrayType = LoadConstant("type_JSArray__JS_ARRAY_TYPE");
  kCodeType = LoadConstant("type_Code__CODE_TYPE");
  kJSFunctionType = LoadConstant("type_JSFunction__JS_FUNCTION_TYPE");
  kFixedArrayType = LoadConstant("type_FixedArray__FIXED_ARRAY_TYPE");
  kJSArrayBufferType = LoadConstant("type_JSArrayBuffer__JS_ARRAY_BUFFER_TYPE");
  kJSTypedArrayType = LoadConstant("type_JSTypedArray__JS_TYPED_ARRAY_TYPE");
}

}  // namespace constants
}  // namespace v8
}  // namespace llnode
