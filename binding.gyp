{
  "targets": [{
    "target_name": "test_addon",
    "include_dirs": [
      "<!(node -e \"require('nan')\")",
    ],
    "sources": [
      "test/addon/src/test_addon.cc",
    ],

    # We need breakpoints
    "cflags": [ "-g", "-O0" ],
    "xcode_settings": {
      "GCC_OPTIMIZATION_LEVEL": "0", # stop gyp from defaulting to -Os
    },
  }],
}
