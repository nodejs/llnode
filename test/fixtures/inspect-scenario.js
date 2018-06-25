'use strict';

const common = require('../common');

const zlib = require('zlib');

let outerVar = 'outer variable';

exports.holder = {};

function makeThin(a, b) {
  var str = a + b;
  var obj = {};
  obj[str];  // Turn the cons string into a thin string.
  return str;
}

function closure() {

  function Class() {
    this.x = 1;
    this.y = 123.456;

    this.hashmap = {};
  }

  Class.prototype.method = function method() {
    throw new Error('Uncaught');
  };

  const c = new Class();

  c.hashmap['some-key'] = 42;
  c.hashmap['other-key'] = 'ohai';
  c.hashmap['utf8-string'] = '你是个好人';
  c.hashmap['cons-string'] =
      'this could be a bit smaller, but v8 wants big str.';
  c.hashmap['cons-string'] += c.hashmap['cons-string'];
  c.hashmap['internalized-string'] = 'foobar';
  // This thin string points to the previous 'foobar'.
  c.hashmap['thin-string'] = makeThin('foo', 'bar');
  // Create an externalized string and slice it.
  c.hashmap['externalized-string'] =
      'string that will be externalized and sliced';
  externalizeString(c.hashmap['externalized-string']);
  // Sliced strings need to be longer that SlicedString::kMinLength
  // which seems to be 13 so our string is 26.
  c.hashmap['sliced-externalized-string'] =
      c.hashmap['externalized-string'].substring(10,36);

  c.hashmap['array'] = [true, 1, undefined, null, 'test', Class];
  c.hashmap['long-array'] = new Array(20).fill(5);
  c.hashmap['array-buffer'] = new Uint8Array(
    [0x01, 0x02, 0x03, 0x04, 0x05]
  ).buffer;
  c.hashmap['uint8-array'] = new Uint8Array(
    [0x01, 0x40, 0x60, 0x80, 0xf0, 0xff]
  );
  c.hashmap['buffer'] = Buffer.from([0xff, 0xf0, 0x80, 0x0f, 0x01, 0x00]);

  c.hashmap[0] = null;
  c.hashmap[4] = undefined;
  c.hashmap[23] = /regexp/;
  c.hashmap[25] = (a,b)=>{a+b};

  let scopedVar = 'scoped value';
  let scopedAPI = zlib.createDeflate()._handle;
  let scopedArray = [ 0, scopedAPI ];

  exports.holder = scopedAPI;

  c.hashmap.scoped = function name() {
    return scopedVar + outerVar + scopedAPI + scopedArray;
  };

  c.method();
}

closure();
