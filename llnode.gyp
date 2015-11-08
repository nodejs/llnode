{
  "targets": [{
    "target_name": "llnode",
    "type": "shared_library",
    "product_prefix": "",

    "include_dirs": [
      ".",
      "<(lldb_dir)/include",
    ],

    "sources": [
      "src/llnode.cc",
      "src/llv8.cc",
      "src/llv8-constants.cc",
    ],

    "conditions": [
      [ "OS == 'mac'", {
        "variables": {
          "mac_shared_frameworks": "/Applications/Xcode.app/Contents/SharedFrameworks",
        },
        "xcode_settings": {
          "OTHER_LDFLAGS": [
            "-F<(mac_shared_frameworks)",
            "-Wl,-rpath,<(mac_shared_frameworks)",
            "-framework LLDB",
          ],
        },
      }, {
        "ldflags": [
          "-l<(lldb_lib)",
        ],
      }],
    ],
  }]
}
