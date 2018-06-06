<p align="left">
  <a href="https://github.com/nodejs/llnode">
    <img
      alt="llnode"
      src="logo.svg"
      width="400"
    />
  </a>
</p>

[![npm](https://img.shields.io/npm/v/llnode.svg?style=flat-square)](https://npmjs.org/package/llnode)


Node.js v4.x+ C++ plugin for the [LLDB](http://lldb.llvm.org) debugger.

The llnode plugin adds the ability to inspect JavaScript stack frames, objects,
source code and more to the standard C/C++ debugging facilities when working
with Node.js processes or core dumps in LLDB.

## Demo

https://asciinema.org/a/29589

## Build Status

| Version | v6.x                    | v8.x                    | v9.x                    | v10.x                     | master                        | v8-canary                        |
|---------|-------------------------|-------------------------|-------------------------|---------------------------|-------------------------------|----------------------------------|
| **Trusty**  | [![v6.x badge][v6-trusty-badge]][travis] | [![v8.x badge][v8-trusty-badge]][travis] | [![v9.x badge][v9-trusty-badge]][travis] | [![v10.x badge][v10-trusty-badge]][travis] | [![master badge][master-trusty-badge]][travis] | [![v8-canary badge][canary-trusty-badge]][travis] |
| **OS X**  | [![v6.x badge][v6-osx-badge]][travis] | [![v8.x badge][v8-osx-badge]][travis] | [![v9.x badge][v9-osx-badge]][travis] | [![v10.x badge][v10-osx-badge]][travis] | - | - |

We have nightly test runs against all Node.js active release lines. We also test
against Node.js master and Node.js v8-canary nightly builds to help us identify
breaking changes on Node.js and V8 before they land on an active release line.

## Quick start

Start an LLDB session with the llnode plugin automatically loaded:

```bash
npm install -g llnode
llnode `which node` -c /path/to/core/dump
```

- For more details on starting llnode see the [Usage](#usage) section.
- To get started with the llnode commands see the [Commands](#commands) section.

## Install Instructions

### Prerequisites: Install LLDB and its Library

To use llnode you need to have the LLDB debugger installed. The recommended
version is LLDB 3.9 and above.

- OS X/macOS
  - You can install [Xcode](https://developer.apple.com/xcode/) and use the
    LLDB that comes with it.
  - Optionally, you can install newer versions of lldb using Homebrew with

    ```bash
    brew update && brew install --with-lldb --with-toolchain llvm
    ```

    and make sure `/usr/local/opt/llvm/bin` gets searched before `/usr/bin/`
    on your `PATH`. llnode will link to the LLDB installation returned by
    `llvm-config` if available.
- Linux
  - You can install the lldb package using the package manager of your
    distribution. You may need to install additional packages for `liblldb`
    as well.
  - For example, on Ubuntu 16.04 you can install the prerequisites with

    ```bash
    apt-get install lldb-4.0 liblldb-4.0-dev
    ```
- FreeBSD

  ```bash
  # Install llvm with lldb and headers
  pkg install llvm39
  rm -f /usr/bin/lldb
  ln -s /usr/local/bin/lldb39 /usr/bin/lldb
  ```

### Install the Plugin

#### Install llnode globally via npm

If you have `lldb` available on your `PATH`, simply run:

```bash
npm install -g llnode
```

To build llnode against a specific lldb version matching an lldb executable,
use the `--lldb_exe` npm option. For example, on Linux the executable on the
`PATH` might be `lldb-3.9`:

```bash
npm install --lldb_exe=`which lldb-3.9` -g llnode
```

After installing with npm, `llnode` should be available on your `PATH` as a
shortcut for running LLDB with the llnode plugin.

#### Optional: Install with Homebrew (OS X/macOS)

```bash
brew install llnode
```

## Loading the llnode Plugin

There are several ways to load the llnode plugin:

### 1. Using the llnode shortcut

If you install llnode globally via npm (`npm install -g llnode`), you can use
the `llnode` shortcut script. This starts `lldb` and automatically issues
the `plugin load` command. All parameters to the llnode script are passed
directly to lldb. If you it's not a local installation, the shortcut will be in
`node_modules/.bin/llnode`.

### 2. Using `~/.lldbinit` to load the Plugin Automatically

To tell LLDB to load llnode automatically regardless of the
version of lldb that you are running, add this line to `~/.lldbinit`:

```
plugin load /path/to/the/llnode/plugin
```

The path to the llnode plugin should be printed when the installation
is finished. On OS X/macOS the plugin is typically
`node_modules/llnode/llnode.dylib`, on Linux it's
`node_modules/llnode/llnode.so`.

### 3. Loading the Plugin Manually

The llnode plugin can also be manually loaded into LLDB using the
`plugin load` command within lldb.

It does not matter whether the `plugin load` command is issued before or after
loading a core dump or attaching to a process.

### 4. Install the Plugin to the LLDB System Plugin Directory

Similar to the `~/.lldbinit` approach, this way LLDB will also load the plugin
automatically on start-up. Doing this may require additional permissions
to be able to copy the plugin libary to the system plugin directory.

On Linux, run `make install-linux` in the project directory, or if
installing with npm, copy `node_modules/llnode/llnode.so`
to `/usr/lib/lldb/plugins` or create a link there.

On OS X/macOS, run `make install-osx` in the project directory, or if
installing with npm, copy `node_modules/llnode/llnode.dylib`
to `~/Library/Application\ Support/LLDB/PlugIns/` or create a link there.

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
      findjsinstances -- List every object with the specified type name.
                         Use -v or --verbose to display detailed `v8 inspect` output for each object.
                         Accepts the same options as `v8 inspect`
      findjsobjects   -- List all object types and instance counts grouped by typename and sorted by instance count. Use
                         -d or --detailed to get an output grouped by type name, properties, and array length, as well as
                         more information regarding each type.
                         With lldb < 3.9, requires the `LLNODE_RANGESFILE` environment variable to be set to a file
                         containing memory ranges for the core file being debugged.
                         There are scripts for generating this file on Linux and Mac in the scripts directory of the llnode
                         repository.
      findrefs        -- Finds all the object properties which meet the search criteria.
                         The default is to list all the object properties that reference the specified value.
                         Flags:

                          * -v, --value expr     - all properties that refer to the specified JavaScript object (default)
                          * -n, --name  name     - all properties with the specified name
                          * -s, --string string  - all properties that refer to the specified JavaScript string value

      getactivehandles  -- Print all pending handles in the queue. Equivalent to running process._getActiveHandles() on
                           the living process.

      getactiverequests -- Print all pending handles in the queue. Equivalent to running process._getActiveHandles() on
                           the living process.

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

## Develop and Test

### Configure and Build

The easiest way to build the plugin:

```bash
# Clone this repo
git clone https://github.com/nodejs/llnode.git && cd llnode

# Configure and build the plugin
npm install
# Or run
make plugin
```

To configure the build yourself:

```bash
# Detect available lldb installation and download headers if necessary
node scripts/configure.js

# To configure with the detected lldb installation
node-gyp configure
# To configure with a specified path to headers, where `$lldb_include_dir`
# contains the <lldb/*/*.h> headers
node-gyp configure -- -Dlldb_include_dir=/usr/local/Cellar/llvm/5.0.0/include
# To configure with a specified path to the libraries, where `$lldb_lib_dir`
# contains `liblldb.so` or `liblldb.dylib`
node-gyp configure -- -Dlldb_lib_dir=/usr/lib/llvm-3.9/lib

# Build the plugin
node-gyp build

# Move the built plugin to the project directory
node scripts/cleanup.js
```

### Test

To run the tests, if `lldb` is an executable on the `PATH`:

```bash
npm run test
```

If the LLDB executable is named differently, point `TEST_LLDB_BINARY`
to it:

```bash
TEST_LLDB_BINARY=`which lldb-4.0` npm run test
```

### Useful Environment Variables

* `LLNODE_DEBUG=true` to see additional debug info from llnode
* `TEST_LLNODE_DEBUG=true` to see additional debug info coming from the tests
* `LLNODE_CORE=/path/to/core/dump LLNODE_NODE_EXE=/path/to/node LLNODE_NO_RANGES=true`
  to use a prepared core dump instead of generating one on-the-fly when running
  the tests.

For example, to inspect the process of `inspect-scenario.js`, run:

```bash
LLNODE_DEBUG=true lldb -- \
  node --abort_on_uncaught_exception --expose_externalize_string \
  test/fixtures/inspect-scenario.js
(lldb) run
```

To debug `test/scan-test.js` with a prepared core dump:

```
LLNODE_DEBUG=true TEST_LLNODE_DEBUG=true \
  LLNODE_CORE=/path/to/core/dump/of/inspect/scenario.js \
  LLNODE_NODE_EXE=/path/to/node \
  LLNODE_NO_RANGES=true \
  node test/scan-test.js
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

[travis]: https://travis-ci.org/nodejs/llnode
[v6-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/1
[v8-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/2
[v9-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/3
[v10-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/4

[v6-osx-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/5
[v8-osx-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/6
[v9-osx-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/7
[v10-osx-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/8

[master-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/9
[canary-trusty-badge]: https://travis-matrix-badges.herokuapp.com/repos/nodejs/llnode/branches/master/10
