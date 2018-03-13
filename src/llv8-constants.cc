#include <stdlib.h>
#include <string.h>

#include <cinttypes>
#include <string>

#include <lldb/API/SBExpressionOptions.h>

#include "src/llv8-constants.h"
#include "src/llv8.h"

namespace llnode {
namespace v8 {
namespace constants {

using lldb::SBAddress;
using lldb::SBError;
using lldb::SBProcess;
using lldb::SBSymbol;
using lldb::SBSymbolContext;
using lldb::SBSymbolContextList;
using lldb::SBTarget;
using lldb::addr_t;

void Module::Assign(SBTarget target, Common* common) {
  loaded_ = false;
  target_ = target;
  common_ = common;
}


void Common::Load() {
  kPointerSize = 1 << LoadConstant("PointerSizeLog2");
  kVersionMajor = LoadRawConstant("v8::internal::Version::major_");
  kVersionMinor = LoadRawConstant("v8::internal::Version::minor_");
  kVersionPatch = LoadRawConstant("v8::internal::Version::patch_");
}


bool Common::CheckHighestVersion(int64_t major, int64_t minor, int64_t patch) {
  Load();

  if (kVersionMajor < major) return true;
  if (kVersionMajor > major) return false;

  if (kVersionMinor < minor) return true;
  if (kVersionMinor > minor) return false;

  if (kVersionPatch > patch) return false;
  return true;
}


bool Common::CheckLowestVersion(int64_t major, int64_t minor, int64_t patch) {
  Load();

  if (kVersionMajor > major) return true;
  if (kVersionMajor < major) return false;

  if (kVersionMinor > minor) return true;
  if (kVersionMinor < minor) return false;

  if (kVersionPatch < patch) return false;
  return true;
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
  Error err;
  kInstanceAttrsOffset =
      LoadConstant("class_Map__instance_attributes__int", err);
  if (err.Fail()) {
    kInstanceAttrsOffset = LoadConstant("class_Map__instance_type__uint16_t");
    kMapTypeMask = 0xffff;
  } else {
    kMapTypeMask = 0xff;
  }

  kMaybeConstructorOffset =
      LoadConstant("class_Map__constructor_or_backpointer__Object",
                   "class_Map__constructor__Object");
  kInstanceDescriptorsOffset =
      LoadConstant("class_Map__instance_descriptors__DescriptorArray");
  kBitField3Offset =
      LoadConstant("class_Map__bit_field3__int", "class_Map__bit_field3__SMI");
  kInObjectPropertiesOffset = LoadConstant(
      "class_Map__inobject_properties_or_constructor_function_index__int",
      "class_Map__inobject_properties__int");
  if (kInObjectPropertiesOffset == -1) {
    kInObjectPropertiesStartOffset = LoadConstant(
        "class_Map__inobject_properties_start_or_constructor_function_index__"
        "char");
  }

  kInstanceTypeOffset = LoadConstant("class_Map__instance_type__uint16_t");
  kInstanceSizeOffset = LoadConstant("class_Map__instance_size__int",
                                     "class_Map__instance_size_in_words__char");
  kDictionaryMapShift = LoadConstant("bit_field3_dictionary_map_shift",
                                     "bit_field3_is_dictionary_map_shift");
  kNumberOfOwnDescriptorsShift =
      LoadConstant("bit_field3_number_of_own_descriptors_shift");
  kNumberOfOwnDescriptorsMask =
      LoadConstant("bit_field3_number_of_own_descriptors_mask");

  if (kNumberOfOwnDescriptorsShift == -1) {
    // TODO(indutny): check v8 version?
    static const int64_t kDescriptorIndexBitCount = 10;

    kNumberOfOwnDescriptorsShift = kDictionaryMapShift;
    kNumberOfOwnDescriptorsShift -= kDescriptorIndexBitCount;

    kNumberOfOwnDescriptorsMask = (1 << kDescriptorIndexBitCount) - 1;
    kNumberOfOwnDescriptorsMask <<= kNumberOfOwnDescriptorsShift;
  }
}


void JSObject::Load() {
  kPropertiesOffset =
      LoadConstant("class_JSReceiver__raw_properties_or_hash__Object",
                   "class_JSReceiver__properties__FixedArray");

  if (kPropertiesOffset == -1)
    kPropertiesOffset = LoadConstant("class_JSObject__properties__FixedArray");

  kElementsOffset = LoadConstant("class_JSObject__elements__Object");
  kInternalFieldsOffset =
      LoadConstant("class_JSObject__internal_fields__uintptr_t");

  if (kInternalFieldsOffset == -1) {
    common_->Load();

    // TODO(indutny): check V8 version?
    kInternalFieldsOffset = kElementsOffset + common_->kPointerSize;
  }
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
  kContextOffset = LoadConstant("class_JSFunction__context__Context");

  if (kContextOffset == -1) {
    common_->Load();

    // TODO(indutny): check V8 version?
    kContextOffset = kSharedInfoOffset + common_->kPointerSize;
  }
}


void JSRegExp::Load() {
  kSourceOffset = LoadConstant("class_JSRegExp__source__Object");
}


void JSDate::Load() {
  kValueOffset = LoadConstant("class_JSDate__value__Object");
};


void SharedInfo::Load() {
  kNameOrScopeInfoOffset =
      LoadConstant("class_SharedFunctionInfo__name_or_scope_info__Object");
  kNameOffset = LoadConstant("class_SharedFunctionInfo__raw_name__Object",
                             "class_SharedFunctionInfo__name__Object");
  kInferredNameOffset =
      LoadConstant("class_SharedFunctionInfo__inferred_name__String",
                   "class_SharedFunctionInfo__function_identifier__Object");
  kScriptOffset = LoadConstant("class_SharedFunctionInfo__script__Object");
  kStartPositionOffset =
      LoadConstant("class_SharedFunctionInfo__start_position_and_type__int",
                   "class_SharedFunctionInfo__start_position_and_type__SMI");
  kEndPositionOffset =
      LoadConstant("class_SharedFunctionInfo__end_position__int",
                   "class_SharedFunctionInfo__end_position__SMI");
  kParameterCountOffset = LoadConstant(
      "class_SharedFunctionInfo__internal_formal_parameter_count__int",
      "class_SharedFunctionInfo__internal_formal_parameter_count__SMI");

  if (kParameterCountOffset == -1) {
    kParameterCountOffset =
        LoadConstant("class_SharedFunctionInfo__formal_parameter_count__SMI");
  }

  // NOTE: Could potentially be -1 on v4 and v5 node, should check in llv8
  kScopeInfoOffset =
      LoadConstant("class_SharedFunctionInfo__scope_info__ScopeInfo");

  kStartPositionMask = LoadConstant("sharedfunctioninfo_start_position_mask");
  kStartPositionShift = LoadConstant("sharedfunctioninfo_start_position_shift");

  if (kStartPositionShift == -1) {
    // TODO(indutny): check version?
    kStartPositionShift = 2;
    kStartPositionMask = ~((1 << kStartPositionShift) - 1);
  }

  if (LoadConstant("class_SharedFunctionInfo__compiler_hints__int") == -1 &&
      kNameOrScopeInfoOffset == -1)
    kEndPositionShift = 1;
  else
    kEndPositionShift = 0;
}


void Code::Load() {
  kStartOffset = LoadConstant("class_Code__instruction_start__uintptr_t");
  kSizeOffset = LoadConstant("class_Code__instruction_size__int");
}


void ScopeInfo::Load() {
  kParameterCountOffset = LoadConstant("scopeinfo_idx_nparams");
  kStackLocalCountOffset = LoadConstant("scopeinfo_idx_nstacklocals");
  kContextLocalCountOffset = LoadConstant("scopeinfo_idx_ncontextlocals");
  kVariablePartIndex = LoadConstant("scopeinfo_idx_first_vars");
}


void Context::Load() {
  kClosureIndex =
      LoadConstant("class_Context__closure_index__int", "context_idx_closure");
  kPreviousIndex =
      LoadConstant("class_Context__previous_index__int", "context_idx_prev");
  // TODO (mmarchini) change LoadConstant to accept variable arguments, a list
  // of constants or a fallback list).
  kNativeIndex =
      LoadConstant("class_Context__native_index__int", "context_idx_native");
  if (kNativeIndex == -1) {
    kNativeIndex = LoadConstant("class_Context__native_context_index__int");
  }
  kEmbedderDataIndex = LoadConstant("context_idx_embedder_data", (int)5);

  kMinContextSlots = LoadConstant("class_Context__min_context_slots__int",
                                  "context_min_slots");
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
  kThinStringTag = LoadConstant("ThinStringTag");

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

void ThinString::Load() {
  kActualOffset = LoadConstant("class_ThinString__actual__String");
}

void FixedArrayBase::Load() {
  kLengthOffset = LoadConstant("class_FixedArrayBase__length__SMI");
}


void FixedArray::Load() {
  kDataOffset = LoadConstant("class_FixedArray__data__uintptr_t");
}


void FixedTypedArrayBase::Load() {
  kBasePointerOffset =
      LoadConstant("class_FixedTypedArrayBase__base_pointer__Object");
  kExternalPointerOffset =
      LoadConstant("class_FixedTypedArrayBase__external_pointer__Object");
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
  if (kBackingStoreOffset == -1) {
    common_->Load();

    kBackingStoreOffset = kByteLengthOffset + common_->kPointerSize;
  }

  kBitFieldOffset = kBackingStoreOffset + common_->kPointerSize;
  if (common_->kPointerSize == 8) kBitFieldOffset += 4;

  kWasNeuteredMask = LoadConstant("jsarray_buffer_was_neutered_mask");
  kWasNeuteredShift = LoadConstant("jsarray_buffer_was_neutered_shift");

  if (kWasNeuteredMask == -1) {
    // TODO(indutny): check V8 version?
    kWasNeuteredMask = 1 << 3;
    kWasNeuteredShift = 3;
  }
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

  if (kPropertyTypeMask == -1) {  // node.js >= 8
    kPropertyAttributesMask = LoadConstant("prop_attributes_mask");
    kPropertyAttributesShift = LoadConstant("prop_attributes_shift");
    kPropertyAttributesEnum_NONE = LoadConstant("prop_attributes_NONE");
    kPropertyAttributesEnum_READ_ONLY =
        LoadConstant("prop_attributes_READ_ONLY");
    kPropertyAttributesEnum_DONT_ENUM =
        LoadConstant("prop_attributes_DONT_ENUM");
    kPropertyAttributesEnum_DONT_DELETE =
        LoadConstant("prop_attributes_DONT_ENUM");

    kPropertyKindMask = LoadConstant("prop_kind_mask");
    kPropertyKindEnum_kAccessor = LoadConstant("prop_kind_Accessor");
    kPropertyKindEnum_kData = LoadConstant("prop_kind_Data");

    kPropertyLocationMask = LoadConstant("prop_location_mask");
    kPropertyLocationShift = LoadConstant("prop_location_shift");
    kPropertyLocationEnum_kDescriptor =
        LoadConstant("prop_location_Descriptor");
    kPropertyLocationEnum_kField = LoadConstant("prop_location_Field");
  } else {  // node.js <= 7
    kFieldType = LoadConstant("prop_type_field");
    kConstFieldType = LoadConstant("prop_type_const_field");
    if (kConstFieldType == -1) {
      // TODO(indutny): check V8 version?
      kConstFieldType = kFieldType | 0x2;
    }
  }

  kRepresentationShift = LoadConstant("prop_representation_shift");
  kRepresentationMask = LoadConstant("prop_representation_mask");

  if (kRepresentationShift == -1) {
    // TODO(indutny): check V8 version?
    kRepresentationShift = 5;
    kRepresentationMask = ((1 << 4) - 1) << kRepresentationShift;
  }

  kRepresentationDouble = LoadConstant("prop_representation_double");
  if (kRepresentationDouble == -1) {
    // TODO(indutny): check V8 version?
    kRepresentationDouble = 7;
  }

  kFirstIndex = LoadConstant("prop_idx_first");
  kSize = LoadConstant("prop_desc_size");
}


void NameDictionary::Load() {
  // TODO(indutny): move this to postmortem
  kKeyOffset = 0;
  kValueOffset = 1;

  kEntrySize = LoadConstant("class_NameDictionaryShape__entry_size__int",
                            "namedictionaryshape_entry_size");

  kPrefixStartIndex =
      LoadConstant("class_NameDictionary__prefix_start_index__int",
                   "namedictionary_prefix_start_index");
  if (kPrefixStartIndex == -1) {
    // TODO(indutny): check V8 version?
    kPrefixStartIndex = kEntrySize;
  }

  kPrefixSize = LoadConstant("class_NameDictionaryShape__prefix_size__int",
                             "namedictionaryshape_prefix_size") +
                kPrefixStartIndex;
}


void Frame::Load() {
  kContextOffset = LoadConstant("off_fp_context");
  kFunctionOffset = LoadConstant("off_fp_function");
  kArgsOffset = LoadConstant("off_fp_args");

  // NOTE: Starting from 5.1.71 these two reside in the same field
  kMarkerOffset = LoadConstant("off_fp_marker", "off_fp_context");

  kAdaptorFrame = LoadConstant("frametype_ArgumentsAdaptorFrame");
  kEntryFrame = LoadConstant("frametype_EntryFrame");
  kEntryConstructFrame = LoadConstant("frametype_ConstructEntryFrame",
                                      "frametype_EntryConstructFrame");
  kExitFrame = LoadConstant("frametype_ExitFrame");
  kInternalFrame = LoadConstant("frametype_InternalFrame");
  kConstructFrame = LoadConstant("frametype_ConstructFrame");
  // NOTE: The JavaScript frame type was removed in V8 6.3.158.
  kJSFrame = LoadConstant("frametype_JavaScriptFrame");
  kOptimizedFrame = LoadConstant("frametype_OptimizedFrame");
  kStubFrame = LoadConstant("frametype_StubFrame");
}


void Types::Load() {
  kFirstNonstringType = LoadConstant("FirstNonstringType");
  kFirstJSObjectType =
      LoadConstant("type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE");

  kHeapNumberType = LoadConstant("type_HeapNumber__HEAP_NUMBER_TYPE");
  kMapType = LoadConstant("type_Map__MAP_TYPE");
  kGlobalObjectType =
      LoadConstant("type_JSGlobalObject__JS_GLOBAL_OBJECT_TYPE");
  kGlobalProxyType = LoadConstant("type_JSGlobalProxy__JS_GLOBAL_PROXY_TYPE");
  kOddballType = LoadConstant("type_Oddball__ODDBALL_TYPE");
  kJSObjectType = LoadConstant("type_JSObject__JS_OBJECT_TYPE");
  kJSAPIObjectType = LoadConstant("APIObjectType");
  kJSSpecialAPIObjectType =
      LoadConstant("SpecialAPIObjectType", "APISpecialObjectType");
  kJSArrayType = LoadConstant("type_JSArray__JS_ARRAY_TYPE");
  kCodeType = LoadConstant("type_Code__CODE_TYPE");
  kJSFunctionType = LoadConstant("type_JSFunction__JS_FUNCTION_TYPE");
  kFixedArrayType = LoadConstant("type_FixedArray__FIXED_ARRAY_TYPE");
  kJSArrayBufferType = LoadConstant("type_JSArrayBuffer__JS_ARRAY_BUFFER_TYPE");
  kJSTypedArrayType = LoadConstant("type_JSTypedArray__JS_TYPED_ARRAY_TYPE");
  kJSRegExpType = LoadConstant("type_JSRegExp__JS_REGEXP_TYPE");
  kJSDateType = LoadConstant("type_JSDate__JS_DATE_TYPE");
  kSharedFunctionInfoType =
      LoadConstant("type_SharedFunctionInfo__SHARED_FUNCTION_INFO_TYPE");
  kScriptType = LoadConstant("type_Script__SCRIPT_TYPE");

  if (kJSAPIObjectType == -1) {
    common_->Load();

    if (common_->CheckLowestVersion(5, 2, 12))
      kJSAPIObjectType = kJSObjectType - 1;
  }
}

}  // namespace constants
}  // namespace v8
}  // namespace llnode
