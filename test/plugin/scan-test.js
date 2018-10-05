'use strict';

const tape = require('tape');
const common = require('../common');
const versionMark = common.versionMark;

tape('v8 findrefs and friends', (t) => {
  t.timeoutAfter(common.saveCoreTimeout);

  // Use prepared core and executable to test
  if (process.env.LLNODE_CORE && process.env.LLNODE_NODE_EXE) {
    test(process.env.LLNODE_NODE_EXE, process.env.LLNODE_CORE, t);
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
    t.ok(/\d+ Class/.test(lines.join('\n')), 'Class should be in findjsobjects');

    sess.send('v8 findjsobjects -d');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);
    t.ok(/3 +0 Class: x, y, hashmap/.test(lines.join('\n')),
         '"Class: x, y, hashmap" should be in findjsobjects -d');

    sess.send('v8 findjsinstances Class_B')
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);

    t.ok((lines.join('\n').match(/<Object: Class_B>/g)).length == 10, 'Should show 10 instances');
    t.ok(/\(Showing 1 to 10 of 10 instances\)/.test(lines.join('\n')), 'Should show 1 to 10 ');

    sess.send('v8 findjsinstances -n 5 Class_B');
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);

    t.ok((lines.join('\n').match(/<Object: Class_B>/g)).length == 5, 'Should show 5 instances');
    t.ok(/\.\.\.\.\.\.\.\.\.\./.test(lines.join('\n')), 'Should show that more instances are available');
    t.ok(/\(Showing 1 to 5 of 10 instances\)/.test(lines.join('\n')), 'Should show 1 to 5 ');

    sess.send('v8 findjsinstances -n 5 Class_B');
    sess.send('version');
  });

  sess.linesUntil(versionMark, (err, lines) => {
    t.error(err);

    t.ok((lines.join('\n').match(/<Object: Class_B>/g)).length == 5, 'Should show 5 instances');
    t.notOk(/\.\.\.\.\.\.\.\.\.\./.test(lines.join('\n')), 'Should not show ellipses');
    t.ok(/\(Showing 6 to 10 of 10 instances\)/.test(lines.join('\n')), 'Should show 6 to 10 ');

    sess.send('v8 findjsinstances Zlib');
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

    // Test if LastContextType constat exists
    sess.hasSymbol('v8dbg_LastContextType', (err, hasSymbol) => {
      t.error(err)
      if(hasSymbol)
        t.ok(/Context\.scopedAPI/.test(lines.join('\n')), 'Should find reference #4');
      sess.quit();
      t.end();
    });
  });
}
