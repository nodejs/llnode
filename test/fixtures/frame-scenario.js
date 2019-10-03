// Note: top-level 'use strict' intentionally omitted.
const common = require('../common');

function crasher(unused) {
  'use strict';
  this.foo = arguments;  // Force adaptor frame on Node.js v12+
  process.abort();       // Creates an exit frame.
  return this;           // Force definition of |this|.
}

// Force V8 to use an inferred name instead of saving the variable name as
// FunctionName.
let fnInferredName;
fnInferredName = (() => function () {
  crasher();    // # args < # formal parameters inserts an adaptor frame.
})();

function Module() {
  this.foo = "bar";
}

Module.prototype.fnInferredNamePrototype = function() {
  fnInferredName();
}

function fnFunctionName() {
  (new Module()).fnInferredNamePrototype();
  return this;  // Force definition of |this|.
}

fnFunctionName();
