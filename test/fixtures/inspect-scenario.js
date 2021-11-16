'use strict';

const common = require('../common');

const zlib = require('zlib');

let outerVar = 'outer variable';

const oneSymbol = Symbol("oneSymbol");

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
  // TODO(No9) Removing JSArrayBufferView tests as they currently fail with
  // error: The value xxxxxxxxxxxx is not a valid value
  // In versions of node > 12
  // See https://github.com/nodejs/llnode/issues/375
  // c.hashmap['uint8-array'] = new Uint8Array(
  //   [0x01, 0x40, 0x60, 0x80, 0xf0, 0xff]
  // );
  // c.hashmap['buffer'] = Buffer.from([0xff, 0xf0, 0x80, 0x0f, 0x01, 0x00]);

  c.hashmap['error'] = new Error('test');
  c.hashmap['error'].code = 'ERR_TEST';
  c.hashmap['error'].errno = 1;

  c.hashmap['stringifiedError'] = new Error('test');
  c.hashmap['stringifiedErrorStack'] = c.hashmap['stringifiedError'].stack;

  c.hashmap['promise'] = new Promise(() => {});

  c.hashmap[0] = null;
  c.hashmap[4] = undefined;
  c.hashmap[23] = /regexp/;
  c.hashmap[25] = (a,b)=>{a+b};
  c.hashmap[oneSymbol] = 42;

  let scopedVar = 'scoped value';
  let scopedAPI = zlib.createDeflate()._handle;
  let scopedArray = [ 0, scopedAPI ];

  c.hashmap['date_1'] = new Date('2000-01-01');
  c.hashmap['date_2'] = new Date(1);

  exports.holder = scopedAPI;

  c.hashmap.scoped = function name() {
    return scopedVar + outerVar + scopedAPI + scopedArray;
  };

  function Class_B() {
    this.name = "Class B";
  }

  function Class_C(class_b_array) {
    this.arr = class_b_array;
    this.my_class_c = "My Class C";
  }

  const arr = new Array();
  for(let i=0; i < 10; i++) arr.push(new Class_B());

  let classC = new Class_C(arr);

  c.method();
}

closure();
