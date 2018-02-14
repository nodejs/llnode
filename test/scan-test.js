'use strict';

const tape = require('tape');
const common = require('./common');
const versionMark = common.versionMark;

tape('v8 findrefs and friends', (t) => {
  t.timeoutAfter(common.saveCoreTimeout);

  // Use prepared core and executable to test
  if (process.env.LLNODE_CORE && process.env.LLNODE_NODE_EXE) {
    test(process.env.LLNODE_NODE_EXE, process.env.LLNODE_CORE, t);
  } else if (process.platform === 'linux') {
    t.skip('No `process save-core` on linux');
    t.end();
  } else {
    common.saveCore({
      scenario: 'inspect-scenario.js'
    }, (err) => {
      t.error(err);
      t.ok(true, 'Saved core');

      test(process.execPath, common.core, t);
    });
  }
});

function test(executable, core, t) {
  const sess = common.Session.loadCore(executable, core, (err) => {
    t.error(err);
    t.ok(true, 'Loaded core');

    sess.send('v8 findjsobjects');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);
    t.ok(/\d+ Zlib/.test(lines.join('\n')), 'Zlib should be in findjsobjects');

    sess.send('v8 findjsobjects -d');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);
    t.ok(/0 +0 Zlib/.test(lines.join('\n')), 'Zlib should be in findjsobjects -d');
    t.ok(/1 +0 Zlib: onerror/.test(lines.join('\n')),
         '"Zlib: onerror" should be in findjsobjects -d');

    sess.send('v8 findjsinstances Zlib');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
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

  sess.linesUntil(versionMark, (err, lines) => {
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

    sess.quit();
    t.end();
  });
}
