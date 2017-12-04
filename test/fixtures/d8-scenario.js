'use strict';

// This file is supposed to be run in the d8 shell, so no Node.js API is
// used here.
// To enable post-mortem support in d8, build it with
// v8_postmortem_support = true.
// Run this file with `lldb -- d8 --abort_on_uncaught_exception d8-scenario.js`

class Class {
  constructor() {
    this.x = 1;
    this.y = 123.456;

    this.hashmap = {};
  }

  method(num, str, arr) {
    this.hashmap['some-key'] = num;
    this.hashmap['other-key'] = str;
    this.hashmap['cons-string'] =
      'this could be a bit smaller, but v8 wants big str.';
    this.hashmap['cons-string'] += this.hashmap['cons-string'];
    this.hashmap['internalized-string'] = 'foobar';

    this.hashmap['array'] = [true, 1, undefined, null, 'test', Class];
    this.hashmap['long-array'] = arr;
    this.hashmap['array-buffer'] = new Uint8Array(
      [0x01, 0x02, 0x03, 0x04, 0x05]
    ).buffer;
    this.hashmap['uint8-array'] = new Uint8Array(
      [0x01, 0x40, 0x60, 0x80, 0xf0, 0xff]
    );

    this.hashmap[0] = null;
    this.hashmap[4] = undefined;
    this.hashmap[23] = /regexp/;
    this.hashmap[25] = (a, b) => a + b;
    throw new Error('Uncaught');
  }
}

function closure() {
  const c = new Class();
  c.method(42, 'ohai', new Array(20).fill(5));
}

closure();
