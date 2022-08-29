'use strict';
const tape = require('tape');
const common = require('../common');

function containsLine(lines, re) {
  return lines.some(line => re.test(line.trim()));
}

tape('usage messages', (t) => {
  t.timeoutAfter(30000);

  const sess = common.Session.create('inspect-scenario.js');

  sess.waitBreak((err) => {
    t.error(err);
    sess.send('v8 print');
  });

  sess.stderr.linesUntil(/USAGE/, (err, lines) => {
    t.error(err);
    const re = /^error: USAGE: v8 print expr$/;
    t.ok(containsLine(lines, re), 'print usage message');
    sess.send('v8 findjsinstances');
  });

  sess.stderr.linesUntil(/USAGE/, (err, lines) => {
    t.error(err);
    const re = /^error: USAGE: v8 findjsinstances \[flags\] instance_name$/;
    t.ok(containsLine(lines, re), 'findjsinstances usage message');
    sess.send('v8 findrefs');
  });

  sess.stderr.linesUntil(/USAGE/, (err, lines) => {
    t.error(err);
    const re = /^error: USAGE: v8 findrefs expr$/;
    t.ok(containsLine(lines, re), 'findrefs usage message');
    sess.quit();
    t.end();
  });
});
