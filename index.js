'use strict';

const addon = require('bindings')('addon');
const fromCoredump = addon.fromCoredump;
const LLNodeHeapType = addon.LLNodeHeapType;
const nextInstance = addon.nextInstance;

function *next() {
  let instance;
  while (instance = nextInstance(this)) {
    yield instance;
  }
}

Object.defineProperty(LLNodeHeapType.prototype, 'instances', {
  enumerable: false,
  configurable: false,
  get: function() {
    return {
      [Symbol.iterator]: next.bind(this)
    }
  }
});

module.exports = {
  fromCoredump
}
