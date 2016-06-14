'use strict';

const common = require('../common');

function first() {
  second('second args', 1);
}

function second(arg1, arg2) {
  third('third args', 1, { a: 1 });
}

function third(arg1, arg2, arg3) {
  const c = new Class();

  c.method('method args', 1.23, null);
}

function Class() {
}

Class.prototype.method = function method(arg1, arg2, arg3) {
  throw new Error('Uncaught');
};

first();
