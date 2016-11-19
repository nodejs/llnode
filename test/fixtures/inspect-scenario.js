'use strict';

const common = require('../common');

const zlib = require('zlib');

let outerVar = 'outer variable';

exports.holder = {};

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
  c.hashmap[0] = null;
  c.hashmap[4] = undefined;
  c.hashmap[23] = /regexp/;
  c.hashmap[25] = (a,b)=>{a+b};

  let scopedVar = 'scoped value';
  let scopedAPI = zlib.createDeflate()._handle;

  exports.holder = scopedAPI;

  c.hashmap.scoped = function name() {
    return scopedVar + outerVar + scopedAPI;
  };

  c.method();
}

closure();
