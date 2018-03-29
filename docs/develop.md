
# Develop the llnode plugin

<!-- TOC -->

- [Configure and Build](#configure-and-build)
- [Run the tests](#run-the-tests)
- [Useful Environment Variables](#useful-environment-variables)
- [Running clang-format](#running-clang-format)

<!-- /TOC -->

## Configure and Build

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
# Detect available LLDB installation and download headers if necessary
node scripts/configure.js

# To configure with the detected LLDB installation
./gyp_llnode
# To configure with a specified LLDB installation on OS X/macOS
./gyp_llnode -Dlldb_build_dir=/usr/local/Cellar/llvm/5.0.0
# To configure with a specified LLDB installation on Linux
./gyp_llnode -Dlldb_dir=/usr/lib/llvm-3.9/
# To configure with a specified LLDB installation on FreeBSD
./gyp_llnode -Dlldb_dir=/usr/local/llvm39/

# Build
make -C out/ -j9

# Move the built plugin to the project directory
node scripts/cleanup.js
```

## Run the tests

To run the tests, if `lldb` is an executable on the `PATH`:

```bash
npm run test
```

If the LLDB executable is named differently, point `TEST_LLDB_BINARY`
to it:

```bash
TEST_LLDB_BINARY=`which lldb-4.0` npm run test
```

## Useful Environment Variables

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

## Running clang-format

We use the clang-format tool to format our C++ code. Run `make format` to
format the code base before submitting changes.

You can download clang-format from [LLVM's website](http://releases.llvm.org/download.html).
Extract the xz file and copy `bin/clang-format` to somewhere that can be searched in
your `$PATH` so that `clang-format` is available in the command line.
