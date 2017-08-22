# llnode

[![Build Status](https://secure.travis-ci.org/nodejs/llnode.png)](http://travis-ci.org/nodejs/llnode)

Node.js v4.x-v8.x C++ plugin for the [LLDB](http://lldb.llvm.org) debugger.

The llnode plugin adds the ability to inspect JavaScript stack frames, objects,
source code and more to the standard C/C++ debugging facilities when working
with Node.js processes or core dumps in LLDB.

### Quick start

Start an lldb session with the llnode plugin pre-loaded:

```bash
npm install -g llnode
llnode node -c core
```

- For more details on starting llnode see the [Usage](#usage) section.
- To get started with the llnode commands see the [Commands](#commands) section.

## Demo

https://asciinema.org/a/29589

## Install Instructions

To use llnode you need to have the LLDB debugger installed.

- On OS X lldb is installed as part of Xcode. You will need Xcode both to build and run llnode.
- On Linux install the lldb package using your distributions package manager.

### Global install with npm

```bash
npm install -g llnode
```

To use a particular build of lldb, use the `--lldb_exe` option:

```bash
npm install --lldb_exe=`which lldb-3.9` -g llnode
```

### Install with Homebrew (OS X)

```bash
brew install llnode
```

## Build Instructions

### OS X

```bash
# Clone this repo
git clone https://github.com/nodejs/llnode.git && cd llnode

# Check out source code of the LLDB that is compatible with OS X's default
# lldb
git clone --depth=1 -b release_39 https://github.com/llvm-mirror/lldb.git lldb

# Initialize GYP
git clone https://chromium.googlesource.com/external/gyp.git tools/gyp

# Configure
./gyp_llnode

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

### FreeBSD

```bash
# Clone this repo
git clone https://github.com/nodejs/llnode.git && cd llnode

# Install llvm with lldb and headers
pkg install llvm39
rm -f /usr/bin/lldb
ln -s /usr/local/bin/lldb39 /usr/bin/lldb

# Initialize GYP
git clone https://chromium.googlesource.com/external/gyp.git tools/gyp

# Configure
./gyp_llnode -Dlldb_dir=/usr/local/llvm39/

# Build
gmake -C out/ -j9
```

(The LLDB function ComputeSystemPluginsDirectory is not implemented on FreeBSD.
The plugin library must be loaded manually.)


## Loading the lldb plugin library.

The simplest method is:
```bash
npm install -g llnode
llnode
```

If you do a global install (npm install -g llnode) you can use the `llnode`
shortcut script. This starts `lldb` and automatically issues the `plugin load` command.
All parameters to the llnode script are passed directly to lldb. If you do not do a
local install the shortcut will be in `node_modules/.bin/llnode`

If you run either `make install-linux` or `make install-osx` the plugin will installed
in the LLDB system plugin directory, in which case LLDB will load the plugin
automatically on start-up. Using this may require additional permissions to be able to
copy the plugin libary to the system plugin directory.

The llnode plugin can also be manually loaded into LLDB using the
`plugin load` command within lldb.

It does not matter whether the `plugin load` command is issued before or after
loading a core dump or attaching to a process.

### OS X

```
lldb

(lldb) plugin load ./node_modules/llnode/llnode.dylib
```

To install the plugin in the LLDB system plugin directory, use the
`make install-osx` build step above, or if installing
with npm copy `node_modules/llnode/llnode.dylib` to
` ~/Library/Application\ Support/LLDB/PlugIns/`.

### Linux

```
lldb

(lldb) plugin load ./node_modules/llnode/llnode.so
```
To install the plugin in the LLDB system plugin directory, use the
`make install-linux` build step above, or if installing with
npm copy `node_modules/llnode/llnode.so` to
`/usr/lib/lldb/plugins`.

### FreeBSD

```
lldb

(lldb) plugin load ./node_modules/llnode/llnode.so
```
LLDB does not support the system plugin directory on FreeBSD.

# Usage

To use llnode with a core dump the core dump needs to be loaded into lldb
along with the exact executable that created the core dump. The executable
contains information that lldb and the llnode plugin need to make sense of
the data in the core dump.

To load a core dump when starting llnode use:
```
llnode /path/to/bin/node -c /path/to/core
```
or to load the core dump after starting lldb:
```
(llnode) target create /path/to/bin/node -c /path/to/core
```

To use llnode against a live process:
```
llnode -- /path/to/bin/node script.js
(llnode) run
```
This is ideal for debugging an npm package with native code.
To debug a Node.js crash on uncaught exception:
```
llnode -- /path/to/bin/node --abort_on_uncaught_exception script.js
(llnode) run
```
lldb will stop your process when it crashes. To see where it stopped use the
v8 bt command. See the [Commands](#commands) section below for more commands.

### Commands

```
(llnode) v8 help
     Node.js helpers

Syntax: v8

The following subcommands are supported:

      bt              -- Show a backtrace with node.js JavaScript functions and their args. An optional argument is accepted; if
                         that argument is a number, it specifies the number of frames to display. Otherwise all frames will be
                         dumped.

                         Syntax: v8 bt [number]
      findjsinstances -- List all objects which share the specified map.
                         Accepts the same options as `v8 inspect`
      findjsobjects   -- List all object types and instance counts grouped by typename and sorted by instance count.
                         Requires `LLNODE_RANGESFILE` environment variable to be set to a file containing memory ranges for the
                         core file being debugged.
                         There are scripts for generating this file on Linux and Mac in the scripts directory of the llnode
                         repository.
      findrefs        -- Finds all the object properties which meet the search criteria.
                         The default is to list all the object properties that reference the specified value.
                         Flags:

                          * -v, --value expr     - all properties that refer to the specified JavaScript object (default)
                          * -n, --name  name     - all properties with the specified name
                          * -s, --string string  - all properties that refer to the specified JavaScript string value

      inspect         -- Print detailed description and contents of the JavaScript value.

                         Possible flags (all optional):

                          * -F, --full-string    - print whole string without adding ellipsis
                          * -m, --print-map      - print object's map address
                          * -s, --print-source   - print source code for function objects
                          * -l num, --length num - print maximum of `num` elements from string/array

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
