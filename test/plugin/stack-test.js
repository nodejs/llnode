'use strict';

const tape = require('tape');

const common = require('../common');

tape('v8 stack', (t) => {
  t.timeoutAfter(45000);

  const sess = common.Session.create('stack-scenario.js');
  sess.waitBreak(() => {
    sess.send('v8 bt');
  });

  sess.wait(/stack-scenario.js/, (err, line) => {
    t.error(err);
    t.ok(/method\(this=.*Class.*method args.*Number: 1\.23.*null/.test(line),
         'Class method name and args');

    // TODO(indutny): line numbers are off
    t.ok(/stack-scenario.js:22:41/.test(line),
         'Class method file pos');
  });

  sess.wait(/stack-scenario.js/, (err, line) => {
    t.error(err);
    t.ok(/third\(.*third args.*Smi: 1.*Object/.test(line),
         'Third function name and args');

    // TODO(indutny): line numbers are off
    t.ok(/stack-scenario.js:13:15/.test(line), 'Third function file pos');
  });

  sess.wait(/stack-scenario.js/, (err, line) => {
    t.error(err);
    t.ok(/second\(.*second args.*Smi: 1/.test(line),
         'Second function name and args');

    // TODO(indutny): line numbers are off
    t.ok(/stack-scenario.js:9:16/.test(line), 'Second function file pos');
  });

  sess.wait(/stack-scenario.js/, (err, line) => {
    t.error(err);
    t.ok(/first/.test(line), 'first function name');

    // TODO(indutny): line numbers are off
    t.ok(/stack-scenario.js:5:15/.test(line), 'first function file pos');

    sess.quit();
    t.end();
  });
});
