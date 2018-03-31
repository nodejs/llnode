{
  "includes": [
    "llnode.gypi",
    "config.gypi"
  ],

  "variables": {
      # gyp does not appear to let you test for undefined variables, so define
      # lldb_lib_dir as empty so we can test it later.
      "lldb_lib_dir%": ""
  },

  "targets": [{
    "target_name": "plugin",
    "type": "shared_library",
    "product_prefix": "",

    "include_dirs": [
      ".",
      "<(lldb_header_dir)/include",
    ],

    "sources": [
      "src/llnode.cc",
      "src/llv8.cc",
      "src/llv8-constants.cc",
      "src/llscan.cc",
    ],

    "cflags" : [ "-std=c++11" ],

    "conditions": [
      [ "OS == 'mac'", {
        # Necessary for node v4.x
        "xcode_settings": {
          "OTHER_CPLUSPLUSFLAGS" : [ "-std=c++11", "-stdlib=libc++" ],
          "OTHER_LDFLAGS": [ "-stdlib=libc++" ],
        },

        "conditions": [
          [ "lldb_lib_dir == ''", {
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
          # lldb_lib_dir != ""
          {
            "xcode_settings": {
              "OTHER_LDFLAGS": [
                "-Wl,-rpath,<(lldb_lib_dir)/lib",
                "-L<(lldb_lib_dir)/lib",
                "-l<(lldb_lib)",
              ],
            },
          }],
        ],
      }],
    ]
  },
  {
    "target_name": "install",
    "type":"none",
    "dependencies" : [ "plugin" ]
  }],
}
