'use strict';

// No `process save-core` on linuxes :(
if (process.platform !== 'darwin')
  return;

const tape = require('tape');

const common = require('./common');

tape('v8 findrefs and friends', (t) => {
  t.timeoutAfter(90000);

  const sess = common.Session.create('inspect-scenario.js');

  sess.waitBreak(() => {
    sess.send(`process save-core ${common.core}`);
    // Just a separator
    sess.send('version');
  });

  sess.wait(/lldb\-/, () => {
    t.ok(true, 'Saved core');

    sess.send(`target create -c ${common.core}`);
  });

  sess.wait(/Core file[^\n]+was loaded/, () => {
    t.ok(true, 'Loaded core');

    common.generateRanges((err) => {
      t.error(err, 'generateRanges');
      sess.send('version');
    });
  });

  sess.wait(/lldb\-/, () => {
    t.ok(true, 'Generated ranges');

    sess.send('v8 findjsobjects');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(/lldb\-/, (lines) => {
    t.ok(/\d+ Zlib/.test(lines.join('\n')), 'Zlib should be in findjsobjects');

    sess.send('v8 findjsinstances Zlib');
    // Just a separator
    sess.send('version');
  });

  sess.linesUntil(/lldb\-/, (lines) => {
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

  sess.linesUntil(/lldb\-/, (lines) => {
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

    sess.send('target delete 1');
    sess.quit();
    t.end();
  });
});
