{
  "includes": [
    "llnode.gypi",
    "config.gypi"
  ],

  "variables": {
      # gyp does not appear to let you test for undefined variables, so define
      # lldb_lib_dir as empty so we can test it later.
      "lldb_lib_dir%": "",
      "lldb_lib_so%": ""
  },

  "target_defaults": {
    "include_dirs": [
      "<(module_root_dir)",
      "<(lldb_include_dir)",
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
                "-Wl,-rpath,<(lldb_lib_dir)",
                "-L<(lldb_lib_dir)",
                "-l<(lldb_lib)",
              ],
            },
          }],
        ],
      }]
    ]
  },

  "targets": [{
    "target_name": "plugin",
    "type": "shared_library",
    "sources": [
      "src/llnode.cc",
      "src/llv8.cc",
      "src/llv8-constants.cc",
      "src/llscan.cc",
    ]
  }, {
    "target_name": "addon",
    "type": "loadable_module",
    "include_dirs": [
      "<!(node -e \"require('nan')\")"
    ],
    "sources": [
      "src/addon.cc",
      "src/llnode_module.cc",
      "src/llnode_api.cc",
      "src/llv8.cc",
      "src/llv8-constants.cc",
      "src/llscan.cc"
    ],
    "conditions": [
      [ "OS=='linux' or OS=='freebsd'", {
        "conditions": [
          # If we could not locate the lib dir, then we will have to search
          # from the global search paths during linking and at runtime
          [ "lldb_lib_dir != ''", {
            "ldflags": [
              "-L<(lldb_lib_dir)",
              "-Wl,-rpath,<(lldb_lib_dir)"
            ]
          }],
          # If we cannot find a non-versioned library liblldb(-x.y).so,
          # then we will have to link to the versioned library
          # liblldb(-x.y).so.z loaded by the detected lldb executable
          [ "lldb_lib_so != ''", {
            "libraries": ["<(lldb_lib_so)"]
          }, {
            "libraries": ["-l<(lldb_lib)"]
          }]
        ]
      }]
    ]
  }],
}
