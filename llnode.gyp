{
  "variables": {
      # gyp does not appear to let you test for undefined variables, so define
      # lldb_build_dir as empty so we can test it later.
      "lldb_build_dir%": ""
  },

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
      "src/llscan.cc",
    ],

    "conditions": [
      [ "OS == 'mac'", {
        "conditions": [
          [ "lldb_build_dir == ''", {
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
          },
          # lldb_builddir != ""
          {
            "xcode_settings": {
              "OTHER_LDFLAGS": [
                "-Wl,-rpath,<(lldb_build_dir)/lib",
                "-L<(lldb_build_dir)/lib",
                "-l<(lldb_lib)",
              ],
            },
          }],
        ],
      }],
    ]
  }],
}
