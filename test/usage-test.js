'use strict';
const tape = require('tape');
const common = require('./common');

function removeBlankLines(lines) {
  return lines.filter((line) => { return line.trim() !== ''; });
}

tape('usage messages', (t) => {
  t.timeoutAfter(15000);

  const sess = common.Session.create('inspect-scenario.js');

  sess.waitBreak(() => {
    sess.send('v8 print');
  });

  sess.stderr.linesUntil(/USAGE/, (lines) => {
    t.ok(/^error: USAGE: v8 print expr$/.test(removeBlankLines(lines)[0]),
         'print usage message');
    sess.send('v8 source list');
  });

  sess.stderr.linesUntil(/USAGE/, (lines) => {
    t.ok(/^error: USAGE: v8 source list$/.test(removeBlankLines(lines)[0]),
         'list usage message');
    sess.send('v8 findjsinstances');
  });

  sess.stderr.linesUntil(/USAGE/, (lines) => {
    const re = /^error: USAGE: v8 findjsinstances \[-Fm\] instance_name$/;

    t.ok(re.test(removeBlankLines(lines)[0]),
         'findjsinstances usage message');
    sess.send('v8 findrefs');
  });

  sess.stderr.linesUntil(/USAGE/, (lines) => {
    t.ok(/^error: USAGE: v8 findrefs expr$/.test(removeBlankLines(lines)[0]),
         'findrefs usage message');
    sess.quit();
    t.end();
  });
});
