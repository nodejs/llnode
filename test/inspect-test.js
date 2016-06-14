'use strict';

const tape = require('tape');

const common = require('./common');

tape('v8 inspect', (t) => {
  t.timeoutAfter(15000);

  const sess = common.Session.create('inspect-scenario.js');

  sess.waitBreak(() => {
    sess.send('v8 bt');
  });

  let that = null;
  let fn = null;

  sess.wait(/inspect-scenario.js/, (line) => {
    let match = line.match(/method\(this=(0x[0-9a-f]+)[^\n]+fn=(0x[0-9a-f]+)/i);
    t.ok(match, 'method should have `this`');

    that = match[1];
    fn = match[2];

    sess.send(`v8 inspect ${that}`);
  });

  let hashmap = null;

  sess.wait(/Class/, (line) => {
    t.notEqual(line.indexOf(that), -1, 'addr of `Class` should match');
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/x=<Smi: 1>/.test(lines), '.x smi property');
    t.ok(/y=123.456/.test(lines), '.y heap number property');

    const match = lines.match(/hashmap=(0x[0-9a-f]+):<Object: Object>/i);
    t.ok(match, '.hashmap object property');

    hashmap = match[1];

    sess.send(`v8 inspect ${hashmap}`);
  });

  let regexp = null;
  let cons = null;

  sess.wait(/Object/, (line) => {
    t.notEqual(line.indexOf(hashmap), -1, 'addr of `Object` should match');
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/\[0\]=[^\n]*null/.test(lines), '[0] null element');
    t.ok(/\[4\]=[^\n]*undefined/.test(lines), '[4] undefined element');

    const reMatch = lines.match(
        /\[23\]=(0x[0-9a-f]+):<(?:Object: RegExp|JSRegExp source=\/regexp\/)>/);
    t.ok(reMatch, '[23] RegExp element');
    regexp = reMatch[1];

    t.ok(/.some-key=<Smi: 42>/.test(lines), '.some-key property');
    t.ok(/.other-key=[^\n]*<String: "ohai">/.test(lines),
         '.other-key property');

    const consMatch = lines.match(
        /.cons-string=(0x[0-9a-f]+):<String: "this could be a ...">/);
    t.ok(consMatch, '.cons-string ConsString property');
    cons = consMatch[1];

    sess.send(`v8 inspect ${regexp}`);
    sess.send(`v8 inspect -F ${cons}`);
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/source=\/regexp\//.test(lines) ||
             /\.source=[^\n]*<String: "regexp">/.test(lines),
         'regexp.source');
  });

  sess.linesUntil(/">/, (lines) => {
    lines = lines.join('\n');
    t.notEqual(
        lines.indexOf('this could be a bit smaller, but v8 wants big str.' +
                      'this could be a bit smaller, but v8 wants big str.'),
        -1,
        'cons string content');

    sess.send(`v8 inspect ${fn}`);
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/\(previous\)/.test(lines), 'method.previous');
    t.ok(/scopedVar[^\n]+"scoped value"/.test(lines), 'method.scopedValue');

    let match = lines.match(
        /\(closure\)=(0x[0-9a-f]+)[^\n]+function: closure/i);
    t.ok(match, '`method` should have `closure`');

    sess.send(`v8 inspect ${match[1]}`);
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/outerVar[^\n]+"outer variable"/.test(lines),
         'method.closure.outerVar');

    sess.quit();
    t.end();
  });
});
