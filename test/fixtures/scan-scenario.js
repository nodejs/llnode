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


  let scopedVar = 'scoped value';
  let scopedAPI = zlib.createDeflate()._handle;
  let scopedArray = [ 0, scopedAPI ];

  exports.holder = scopedAPI;

  c.hashmap.scoped = function name() {
    return scopedVar + outerVar + scopedAPI + scopedArray;
  };

  function Class_B() {
    this.my_class_b = "Class B";
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
