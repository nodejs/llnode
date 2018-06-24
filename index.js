'use strict';

const {
  fromCoredump,
  LLNodeHeapType,
  nextInstance
} = require('bindings')('addon');

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
