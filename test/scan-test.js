'use strict';

const tape = require('tape');
const common = require('./common');

tape('v8 findrefs and friends', (t) => {
  t.timeoutAfter(common.saveCoreTimeout);

  // Use prepared core and executable to test
  if (process.env.LLNODE_CORE && process.env.LLNODE_NODE_EXE) {
    test(process.env.LLNODE_NODE_EXE, process.env.LLNODE_CORE, t);
  } else if (process.platform === 'linux') {
    t.skip('No `process save-core` on linux');
    t.end();
  } else {
    saveCoreAndTest(t);
  }
});

function saveCoreAndTest(t) {
  // Create a core and test
  const sess = common.Session.create('inspect-scenario.js');
  sess.timeoutAfter(common.saveCoreTimeout);

  sess.waitBreak(() => {
    sess.send(`process save-core ${common.core}`);
    // Just a separator
    sess.send('version');
  });

  sess.wait(/lldb-/, (err) => {
    t.error(err);
    t.ok(true, 'Saved core');
    sess.send('target delete 0');
    sess.quit();

    test(process.execPath, common.core, t);
  });
}

function test(executable, core, t) {
  let sess, ranges;
  if (process.env.LLNODE_NO_RANGES) {
    sess = common.Session.loadCore(executable, core);
  } else {
    ranges = core + '.ranges';
    sess = common.Session.loadCore(executable, core, ranges);
  }
  sess.timeoutAfter(common.loadCoreTimeout);

  sess.waitCoreLoad((err) => {
    t.error(err);
    t.ok(true, 'Loaded core');

    if (ranges) {
      common.generateRanges(core, ranges, (err) => {
        t.error(err);
        t.ok(true, 'Generated ranges');
        sess.send('version');
      });
    } else {
      sess.send('version');
    }
  });

  sess.wait(/lldb-/, (err) => {
    t.error(err);
    sess.send('v8 findjsobjects');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(/lldb-/, (err, lines) => {
    t.error(err);
    t.ok(/\d+ Zlib/.test(lines.join('\n')), 'Zlib should be in findjsobjects');

    sess.send('v8 findjsinstances Zlib');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(/lldb-/, (err, lines) => {
    t.error(err);
    // Find refs to every Zlib instance
    let found = false;
    for (let i = lines.length - 1; i >= 0; i--) {
      const match = lines[i].match(/(0x[0-9a-f]+):<Object: Zlib>/i);
      if (!match)
        continue;

      found = true;
      sess.send(`v8 findrefs ${match[1]}`);
    }
    t.ok(found, 'Zlib should be in findjsinstances');

    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(/lldb-/, (err, lines) => {
    t.error(err);

    // `class Deflate extends Zlib` makes instances show up as
    // Transform objects (which Zlib inherits from) in node.js 8.0.0.
    // That change was reverted in https://github.com/nodejs/node/pull/13374
    // and released in 8.1.0.
    const re =
        (process.version === 'v8.0.0' ?
            /Transform\._handle/ : /Deflate\._handle/);
    t.ok(re.test(lines.join('\n')), 'Should find reference');
    t.ok(/Object\.holder/.test(lines.join('\n')), 'Should find reference #2');
    t.ok(/\(Array\)\[1\]/.test(lines.join('\n')), 'Should find reference #3');

    sess.send('target delete 0');
    sess.quit();
    t.end();
  });
}
