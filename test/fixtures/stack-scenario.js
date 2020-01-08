'use strict';

function first() {
  second('second args', 1);
}

function second() {
  third('third args', 1, { a: 1 });
}

function third() {
  const c = new Class();

  c.method('method args', 1.23, null);
}

function Class() {
}

Class.prototype.method = function method() {
  throw new Error('Uncaught');
};

first();
