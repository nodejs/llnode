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
[![Build Status](https://img.shields.io/travis/nodejs/llnode.svg?style=flat-square)](https://travis-ci.org/nodejs/llnode)

Node.js v4.x+ C++ plugin for the [LLDB](http://lldb.llvm.org) debugger.

The llnode plugin adds the ability to inspect JavaScript stack frames, objects,
source code and more to the standard C/C++ debugging facilities when working
with Node.js processes or core dumps in LLDB.

## Quick start

Demo: https://asciinema.org/a/29589

With the [prerequisites](docs/installation.md#prerequisites) installed,
start an lldb session with the llnode plugin automatically loaded:

```bash
npm install -g llnode  # installs llnode
# Load a core dump
llnode /path/to/node -c /path/to/core/dump
# Or, run a Node.js script with --abort_on_uncaught_exceptions
llnode -- /path/to/node --abort_on_uncaught_exceptions /path/to/node/script
```

## Install llnode

See [Installing llnode](docs/installation.md).

## Using the llnode plugin

- For more details on starting llnode see
  [Loading the llnode Plugin](docs/usage.md#loading-the-llnode-plugin).
- To get started with the llnode commands see
  [Commands](docs/usage.md#commands).

## Develop and Test

See [Develop the llnode plugin](docs/develop.md).

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
