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
  let thin = null;
  let arrowFunc = null;
  let array = null;
  let longArray = null;
  let arrayBuffer = null;
  let uint8Array = null;
  let buffer = null;

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

    const arrowMatch = lines.match(
    /\[25\]=(0x[0-9a-f]+):<(function: c.hashmap).*>/);
    t.ok(arrowMatch, '[25] Arrow Function element');
    arrowFunc = arrowMatch[1];

    t.ok(/.some-key=<Smi: 42>/.test(lines), '.some-key property');
    t.ok(/.other-key=[^\n]*<String: "ohai">/.test(lines),
         '.other-key property');

    const arrayMatch =
        lines.match(/.array=(0x[0-9a-f]+):<Array: length=6>/);
    t.ok(arrayMatch, '.array JSArray property');
    array = arrayMatch[1];

    const longArrayMatch =
        lines.match(/.long-array=(0x[0-9a-f]+):<Array: length=20>/);
    t.ok(longArrayMatch, '.array JSArray property');
    longArray = longArrayMatch[1];

    const arrayBufferRe = new RegExp('.array-buffer=(0x[0-9a-f]+):' +
      '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5>');
    const arrayBufferMatch = lines.match(arrayBufferRe);
    t.ok(arrayBufferMatch, '.array-buffer JSArrayBuffer property');
    arrayBuffer = arrayBufferMatch[1];

    const uint8ArrayRe = new RegExp('.uint8-array=(0x[0-9a-f]+):' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6>');
    const uint8ArrayMatch = lines.match(uint8ArrayRe);
    t.ok(uint8ArrayMatch, '.uint8-array JSArrayBufferView property');
    uint8Array = uint8ArrayMatch[1];

    const bufferRe = new RegExp('.buffer=(0x[0-9a-f]+):' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6>');
    const bufferMatch = lines.match(bufferRe);
    t.ok(bufferMatch, '.buffer JSArrayBufferView property');
    buffer = bufferMatch[1];

    const consMatch = lines.match(
        /.cons-string=(0x[0-9a-f]+):<String: "this could be a ...">/);
    t.ok(consMatch, '.cons-string ConsString property');
    cons = consMatch[1];

    const thinMatch = lines.match(
        /.thin-string=(0x[0-9a-f]+):<String: "foobar">/);
    t.ok(thinMatch, '.thin-string ThinString property');
    thin = thinMatch[1];

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

    sess.send(`v8 inspect --string-length 20 ${cons}`);
  });

  sess.linesUntil(/">/, (lines) => {
    lines = lines.join('\n');
    t.notEqual(
        lines.indexOf('this could be a bit ...'),
        -1,
        '--string-length truncates the string');

    sess.send(`v8 inspect ${thin}`);
  });

  sess.linesUntil(/">/, (lines) => {
    lines = lines.join('\n');
    t.ok(
      /0x[0-9a-f]+:<String: "foobar">/.test(lines),
      'thin string content');

    sess.send(`v8 inspect ${array}`);
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.notEqual(
        lines.indexOf('<Array: length=6'),
        -1,
        'array length');
    t.ok(
        lines.match(/\[5\]=0x[0-9a-f]+:<function: Class at .+\.js:\d+:\d+>}>$/),
        'array content');
    sess.send(`v8 inspect --array-length 10 ${longArray}`);
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.notEqual(
        lines.indexOf('<Array: length=20'),
        -1,
        'long array length');
      t.ok(
          lines.match(/\[9\]=<Smi: 5>}>$/),
          'long array content');
      sess.send(`v8 inspect ${arrayBuffer}`);
  });

  sess.linesUntil(/\]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5: \\[\n' +
      '  01, 02, 03, 04, 05\n' +
      ']>');
    t.ok(
        re.test(lines),
        'array buffer content');
    sess.send(`v8 inspect --array-length 1 ${arrayBuffer}`);
  });

  sess.linesUntil(/]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5: \\[\n' +
      '  01 ...\n' +
      ']>');
    t.ok(
        re.test(lines),
        'array buffer content with maximum length 1');
    sess.send(`v8 inspect ${uint8Array}`);
  });

  sess.linesUntil(/]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6: \\[\n' +
      '  01, 40, 60, 80, f0, ff\n' +
      ']>');
    t.ok(
        re.test(lines),
        'typed array content');
    sess.send(`v8 inspect --array-length 1 ${uint8Array}`);
  });

  sess.linesUntil(/]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6: \\[\n' +
      '  01 ...\n' +
      ']>');
    t.ok(
        re.test(lines),
        'typed array content with maximum length 1');
    sess.send(`v8 inspect ${buffer}`);
  });

  sess.linesUntil(/]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6: \\[\n' +
      '  ff, f0, 80, 0f, 01, 00\n' +
      ']>');
    t.ok(
        re.test(lines),
        'buffer content');
    sess.send(`v8 inspect --array-length 1 ${buffer}`);
  });

  sess.linesUntil(/]>/, (lines) => {
    lines = lines.join('\n');
    const re = new RegExp(
      '0x[0-9a-f]+:' +
      '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
      'byteLength=6: \\[\n' +
      '  ff ...\n' +
      ']>');
    t.ok(
        re.test(lines),
        'buffer content with maximum length 1');
    sess.send(`v8 inspect -s ${arrowFunc}`);
  });


  sess.linesUntil(/^>/, (lines) => {
    lines = lines.join('\n');
    // Include 'source:' and '>' to act as boundaries. (Avoid
    // passing if the whole file it displayed instead of just
    // the function we want.)
    const arrowSource = 'source:\n' +
        'function c.hashmap.(anonymous function)(a,b)=>{a+b}\n' +
        '>'

    t.ok(lines.includes(
        arrowSource),
        'arrow method source');

    sess.send(`v8 inspect -s ${fn}`);
  });

  sess.linesUntil(/^>/, (lines) => {
    lines = lines.join('\n');

    // Include 'source:' and '>' to act as boundaries. (Avoid
    // passing if the whole file it displayed instead of just
    // the function we want.)
    const methodSource = "  source:\n" +
    "function method() {\n" +
    "    throw new Error('Uncaught');\n" +
    "  }\n" +
    ">"

    t.ok(lines.includes(
        methodSource),
        'method source found');

    if (process.version < 'v5.0.0') {
      sess.quit();
      t.end();
    } else {
      // No Context debugging for older node.js
      t.ok(/\(previous\)/.test(lines), 'method.previous');
      t.ok(/scopedVar[^\n]+"scoped value"/.test(lines), 'method.scopedValue');

      let match = lines.match(
          /scopedAPI=(0x[0-9a-f]+)[^\n]+Zlib/i);
      t.ok(match, '`method` should have `scopedAPI`');

      sess.send(`v8 inspect ${match[1]}`);

      match = lines.match(
          /\(closure\)=(0x[0-9a-f]+)[^\n]+function: closure/i);
      t.ok(match, '`method` should have `closure`');

      sess.send(`v8 inspect ${match[1]}`);
    }
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/internal fields/.test(lines), 'method.scopedAPI.internalFields');
  });

  sess.linesUntil(/}>/, (lines) => {
    lines = lines.join('\n');
    t.ok(/outerVar[^\n]+"outer variable"/.test(lines),
         'method.closure.outerVar');

    sess.quit();
    t.end();
  });
});
