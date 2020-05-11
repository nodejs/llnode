'use strict';

const {
  fromCoredump,
  LLNodeHeapType,
  nextInstance
} = require('bindings')('addon');

const coredump = fromCoredump(process.argv[2], process.argv[0]);
console.log(coredump.getHeapTypes());

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
