# Installing llnode

<!-- TOC -->

- [Prerequisites](#prerequisites)
  - [OS X/macOS](#os-xmacos)
  - [Ubuntu/Debian](#ubuntudebian)
  - [Fedora](#fedora)
  - [CentOS / RHEL](#centos--rhel)
  - [FreeBSD](#freebsd)
- [Install the Plugin](#install-the-plugin)
  - [Install llnode globally via npm](#install-llnode-globally-via-npm)
  - [Alternative: Install with Homebrew (OS X/macOS)](#alternative-install-with-homebrew-os-xmacos)
  - [Alternative: build the plugin from source](#alternative-build-the-plugin-from-source)

<!-- /TOC -->

## Prerequisites

To build llnode, you need:

- Node.js v4.x or later (latest stable is preferred, the version of Node.js
  that you use to build llnode is not relevant to the version of Node.js
  that you need to debug your core dump or script with).
- gyp: if you are installing with npm, you can use node-gyp and install it with
  `npm install -g node-gyp`. If you are from source, you can either use node-gyp
  or in the llnode project directory, run

  ```
  git clone https://chromium.googlesource.com/external/gyp.git tools/gyp
  ```

Other than Node.js and gyp, you also need:

- The lldb debugger and its library, usually available in a
  `lldb` or `llvm` package. The recommended version is lldb 3.9 and above.
  The latest stable version is preferred.
- GNU Make
- Python (v2.7 recommended)
- Usable C and C++ compilers specified with `CC` and `CXX` environment variables

### OS X/macOS

If you have [Xcode](https://developer.apple.com/xcode/) and the command line
tools installed, you can use the lldb and make that come with it.

Optionally, you can install newer versions of lldb and make using Homebrew with

```bash
brew update && brew install make --with-lldb --with-toolchain llvm
```

and make sure `/usr/local/opt/llvm/bin` gets searched before `/usr/bin/`
in your `PATH`. llnode will link to the lldb installation returned by
`llvm-config` if available.

### Ubuntu/Debian

See [LLVM's website](http://apt.llvm.org/) on how to install lldb on
Debian-based distributions.

For example, on Ubuntu 16.04 you can set up the environment with:

```bash
apt-get update && apt-get install clang-4.0 lldb-4.0 liblldb-4.0-dev make
# Configure environment variables for compilers
export CC=clang-4.0
export CXX=clang++-4.0
```

### Fedora

For example, on Fedora 27, you can set up the environment with:

```bash
yum install llvm lldb python clang make
# Or, if you use dnf
dnf install llvm lldb python clang make
# Configure environment variables for compilers
export CC=clang
export CXX=clang++
```

### CentOS / RHEL

For example, on CentOS 7, if you use the Linux Software Collections,
you can set up the environment with:

```bash
yum install centos-release-scl  # This installs the SCL license
yum install llvm-toolset-7
# Enters the bash with llvm-toolset-7 enabled to start installation
# before building llnode
scl enable llvm-toolset-7 bash
# Configure environment variables for compilers
export CC=clang
export CXX=clang++
```

### FreeBSD

For example, on FreeBSD 11, you can set up the environment with:

```bash
# Install llvm with lldb and headers
pkg install llvm50 gmake python
# Remove the link to the default installation which is 4.0 for FreeBSD 11
rm -f /usr/bin/lldb
# Relink to lldb 5.0
ln -s /usr/local/bin/lldb50 /usr/bin/lldb
# Configure environment variables for compilers
export CC=clang
export CXX=clang++
```

## Install the Plugin

### Install llnode globally via npm

If you have `lldb` available on your `PATH`, simply run:

```bash
npm install -g llnode
```

To use a particular build of lldb, use the `--lldb_exe` option. For example,
on Linux the executable on the `PATH` might be `lldb-3.9`:

```bash
npm install --lldb_exe=`which lldb-3.9` -g llnode
```

After installing with npm, `llnode` should be available on your `PATH` as a
shortcut for running lldb with the llnode plugin.

### Alternative: Install with Homebrew (OS X/macOS)

```bash
brew install llnode
```

### Alternative: build the plugin from source

```bash
git clone https://github.com/nodejs/llnode.git
cd llnode && make plugin

# Or, if you are on a BSD, use GNU make instead of the BSD make
# You might need to pkg install gmake if it's not installed
cd llnode && gmake plugin
```
