'use strict';

const tape = require('tape');

const common = require('./common');

function testWorkqueueCommands(t, sess) {
  sess.send('v8 getactivehandles');

  sess.wait(/TCP/, (err, line) => {
    t.error(err);
    let match = line.match(/<Object: TCP/i);
    t.ok(match, 'TCP handler should be an Object');

    sess.send('v8 getactivehandles');
  });

  sess.wait(/Timer/, (err, line) => {
    t.error(err);
    let match = line.match(/<Object: Timer/i);
    t.ok(match, 'Timer handler should be an Object');

    sess.send('v8 getactiverequests');
  });

  sess.wait(/FSReqWrap/, (err, line) => {
    t.error(err);
    let match = line.match(/<Object: FSReqWrap/i);
    t.ok(match, 'FSReqWrap handler should be an Object');

    sess.quit();
    t.end();
  });
}

tape('v8 workqueue commands', (t) => {
  t.timeoutAfter(15000);

  const sess = common.Session.create('workqueue-scenario.js');
  sess.timeoutAfter

  sess.waitBreak((err) => {
    t.error(err);
    sess.send('v8 getactivehandles');
  });

  // Check if current node version support these commands.
  sess.waitError(/error: Couldn't get node's Environment/, (err, line) => {
    if (err) {
      testWorkqueueCommands(t, sess);
    } else {
      // TODO (mmarchini): print node's version from core.
      t.skip(`workqueue commands can't be tested with this version of node`);
      sess.quit();
      t.end();
    }
  }, false, false);
});
