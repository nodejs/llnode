# llnode

[![Build Status](https://secure.travis-ci.org/indutny/llnode.png)](http://travis-ci.org/indutny/llnode)

Node.js v4.x-v6.x C++ plugin for [LLDB](http://lldb.llvm.org) - a next generation, high-performance debugger.

## Demo

https://asciinema.org/a/29589

## Build instructions

### OS X

Easy:
```bash
brew install llnode
```

Harder:
```bash
# Clone this repo
git clone https://github.com/nodejs/llnode.git && cd llnode

# Check out source code of the LLDB that is compatible with OS X's default
# lldb
git clone --depth=1 -b release_38 https://github.com/llvm-mirror/lldb.git lldb

# Initialize GYP
git clone https://chromium.googlesource.com/external/gyp.git tools/gyp

# Configure
./gyp_lldb

# Build
make -C out/ -j9

# Install
make install-osx
```

### Linux

```bash
# Clone this repo
git clone https://github.com/nodejs/llnode.git && cd llnode

# Install lldb and headers
sudo apt-get install lldb-3.8 lldb-3.8-dev

# Initialize GYP
git clone https://chromium.googlesource.com/external/gyp.git tools/gyp

# Configure
./gyp_llnode -Dlldb_dir=/usr/lib/llvm-3.8/

# Build
make -C out/ -j9

# Install
sudo make install-linux
```

### Usage

```
lldb
(lldb) v8 help
     Node.js helpers

Syntax: v8

The following subcommands are supported:

      bt              -- Show a backtrace with node.js JavaScript functions and their args. An optional argument is accepted; if
                         that argument is a number, it specifies the number of frames to display. Otherwise all frames will be
                         dumped.

                         Syntax: v8 bt [number]
      findjsinstances -- List all objects which share the specified map.
                         Accepts the same options as `v8 inspect`
      findjsobjects   -- List all object types and instance counts grouped by map and sorted by instance count.
                         Requires `LLNODE_RANGESFILE` environment variable to be set to a file containing memory ranges for the
                         core file being debugged.
                         There are scripts for generating this file on Linux and Mac in the scripts directory of the llnode
                         repository.
      findrefs        -- Find all the objects that refer to the specified object.
      inspect         -- Print detailed description and contents of the JavaScript value.

                         Possible flags (all optional):

                          * -F, --full-string    - print whole string without adding ellipsis
                          * -m, --print-map      - print object's map address
                          * -s, --print-source   - print source code for function objects
                          * --string-length num  - print maximum of `num` characters in string

                         Syntax: v8 inspect [flags] expr
      nodeinfo        -- Print information about Node.js
      print           -- Print short description of the JavaScript value.

                         Syntax: v8 print expr
      source          -- Source code information

For more help on any particular subcommand, type 'help <command> <subcommand>'.
```


## LICENSE

This software is licensed under the MIT License.

Copyright Fedor Indutny, 2016.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.
