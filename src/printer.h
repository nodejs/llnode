#ifndef SRC_INSPECT_H_
#define SRC_INSPECT_H_

#include <string>

#include <lldb/API/LLDB.h>

#include "src/llv8.h"
#include "src/node.h"

namespace llnode {

class Printer {
 public:
  class PrinterOptions {
   public:
    PrinterOptions()
        : detailed(false),
          print_map(false),
          print_source(false),
          length(kLength),
          indent_depth(1),
          output_limit(0),
          with_args(true) {}

    static const unsigned int kLength = 16;
    static const unsigned int kIndentSize = 2;
    inline std::string get_indent_spaces() const {
      return std::string(indent_depth * kIndentSize, ' ');
    }
    bool detailed;
    bool print_map;
    bool print_source;
    unsigned int length;
    unsigned int indent_depth;
    int output_limit;
    bool with_args;
  };

  Printer(v8::LLV8* llv8) : llv8_(llv8), options_(){};
  Printer(v8::LLV8* llv8, const PrinterOptions options)
      : llv8_(llv8), options_(options){};

  template <typename T, typename Actual = T>
  std::string Stringify(T value, Error& err);

  // JSObject Specific Methods
  std::string StringifyInternalFields(v8::JSObject js_obj, Error& err);
  std::string StringifyProperties(v8::JSObject js_obj, Error& err);

  std::string StringifyElements(v8::JSObject js_obj, Error& err);
  std::string StringifyElements(v8::JSObject js_obj, int64_t length,
                                Error& err);
  std::string StringifyDictionary(v8::JSObject js_obj, Error& err);
  std::string StringifyDescriptors(v8::JSObject js_obj, v8::Map map,
                                   Error& err);

  std::string StringifyJSObjectFields(v8::JSObject js_obj, Error& err);

  // FixedArray Specific Methods
  std::string StringifyContents(v8::FixedArray fixed_array, int length,
                                Error& err);

  // JSFrame Specific Methods
  std::string StringifyArgs(v8::JSFrame js_frame, v8::JSFunction fn,
                            Error& err);

 private:
  v8::LLV8* llv8_;
  const PrinterOptions options_;
};

}  // namespace llnode

#endif  // SRC_LLNODE_H_
