// Note: top-level 'use strict' intentionally omitted.
function crasher() {
  'use strict';
  process.abort();  // Creates an exit frame.
  return this;      // Force definition of |this|.
}

function eyecatcher() {
  crasher();    // # args < # formal parameters inserts an adaptor frame.
  return this;  // Force definition of |this|.
}

eyecatcher();
