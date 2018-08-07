'use strict';

const tape = require('tape');

const common = require('../common');

// <Object: Object elements {
const hashMapTests = {
  // [0]=0x000039e38cc82201:<null>,
  'null': {
    re: /\[0\]=(0x[0-9a-f]+):<null>/,
    desc: '[0] null element'
  },
  // [4]=0x000039e38cc822d1:<undefined>,
  'undefined': {
    re: /\[4\]=(0x[0-9a-f]+):<undefined>/,
    desc: '[4] undefined element'
  },
  // [23]=0x000003df9cbe83d9:<JSRegExp source=/regexp/>,
  'regexp': {
    re:/\[23\]=(0x[0-9a-f]+):<(?:Object: RegExp|JSRegExp source=\/regexp\/)>/,
    desc: '[23] RegExp element',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(/source=\/regexp\//.test(lines) ||
            /\.source=[^\n]*<String: "regexp">/.test(lines),
            'hashmap[23] should have the correct regexp.source');
        cb(null);
      });
    }
  },
  // [25]=0x000036eccf7c0b79:<function: c.hashmap.(anonymous function) at /foo/bar.js:63:19>}
  'arrow': {
    re: /\[25\]=(0x[0-9a-f]+):<(function: c.hashmap).*>/,
    desc: '[25] Arrow Function element',
    validator: (t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect -s ${address}`);

      sess.linesUntil(/^>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        // Include 'source:' and '>' to act as boundaries. (Avoid
        // passing if the whole file it displayed instead of just
        // the function we want.)
        const arrowSource = 'source:\n' +
            'function c.hashmap.(anonymous function)(a,b)=>{a+b}\n' +
            '>';

        t.ok(lines.includes(arrowSource),
            'hashmap[25] should have the correct function source');
        cb(null);
      });
    }
  },
  // properties {
  // .some-key=<Smi: 42>,
  'smi': {
    re: /.some-key=<Smi: 42>/,
    desc: '.some-key property'
  },
  // .other-key=0x000036eccf7bd9c1:<String: "ohai">,
  'string': {
    re: /.other-key=[^\n]*<String: "ohai">/,
    desc: '.other-key property'
  },
  // .cons-string=0x000003df9cbe7579:<String: "this could be a ...">,
  'cons-string': {
    re: /.cons-string=(0x[0-9a-f]+):<String: "this could be a ...">/,
    desc: '.cons-string ConsString property',
    validators: [(t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect -F ${address}`);

      sess.linesUntil(/">/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const expected = 'this could be a bit smaller, but v8 wants big str.' +
                         'this could be a bit smaller, but v8 wants big str.';
        t.ok(lines.includes(expected),
            'hashmap.cons-string should have the right content');
        cb(null);
      });
    }, (t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect --string-length 20 ${address}`);

      sess.linesUntil(/">/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const expected = 'this could be a bit ...';
        t.ok(lines.includes(expected),
            '--string-length should truncate the string');
        cb(null);
      });
    }]
  },
  // .internalized-string=0x000036eccf7bda89:<String: "foobar">,
  'internalized-string': {
    re: /.internalized-string=(0x[0-9a-f]+):<String: "foobar">/,
    desc: '.internalized-string Internalized String property'
  },
  // .thin-string=0x000003df9cbe7621:<String: "foobar">,
  'thin-string': {
    re: /.thin-string=(0x[0-9a-f]+):<String: "foobar">/,
    desc: '.thin-string ThinString property',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/">/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(
            /0x[0-9a-f]+:<String: "foobar">/.test(lines),
            'hashmap.thin-string should have the right content');
        cb(null);
      });
    }
  },
  // .externalized-string=0x000036eccf7bdb41:<String: "(external)">,
  'externalized-string': {
    re: /.externalized-string=(0x[0-9a-f]+):<String: "\(external\)">/,
    desc: '.externalized-string ExternalString property'
  },
  // .sliced-externalized-string=0x000003df9cbe77e9:<String: "(external)">,
  'sliced-externalized-string': {
    re: /.sliced-externalized-string=(0x[0-9a-f]+):<String: "\(external\)">/,
    desc: '.sliced-externalized-string Sliced ExternalString property'
  },
  // .error=0x0000392d5d661119:<Object: Error>
  'error': {
    re: /.error=(0x[0-9a-f]+):<Object: Error>/,
    desc: '.error Error property',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');

        let codeMatch = lines.match(/code=(0x[0-9a-f]+):<String: "ERR_TEST">/i);
        t.ok(codeMatch, 'hashmap.error.code should be "ERR_TEST"');

        let errnoMatch = lines.match(/errno=<Smi: 1>/i);
        t.ok(errnoMatch, 'hashmap.error.errno should be 1');

        let stackMatch  = lines.match(/error stack {/i);
        t.ok(stackMatch, 'Error object should have an error stack');

        let closureMatch = lines.match(/0x[0-9a-f]+:<function: closure/i);
        t.ok(closureMatch, 'closure frame should be in the error stack');

        cb(null);
      });
    }
  },
  // .array=0x000003df9cbe7919:<Array: length=6>,
  'array': {
    re: /.array=(0x[0-9a-f]+):<Array: length=6>/,
    desc: '.array JSArray property',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(lines.includes('<Array: length=6'),
            'length of hashmap.array should be 6');
        cb(null);
      });
    }
  },
  // .long-array=0x000003df9cbe7aa9:<Array: length=20>,
  'long-array': {
    re: /.long-array=(0x[0-9a-f]+):<Array: length=20>/,
    desc: '.long-array JSArray property',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect --array-length 10 ${address}`);

      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(lines.includes('<Array: length=20'),
            'length of hashmap.long-array should be 6');
        t.ok(/\[9\]=<Smi: 5>}>$/.test(lines),
            'hashmap.long-array should have the right content');
        cb(null);
      });
    }
  },
  // .array-buffer=0x000003df9cbe7df1:<ArrayBuffer: backingStore=0x00000000022509b0, byteLength=5>,
  'array-buffer': {
    re: new RegExp('.array-buffer=(0x[0-9a-f]+):' +
                    '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5>'),
    desc: '.array-buffer JSArrayBuffer property',
    validators: [(t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5: \\[\n' +
            '  01, 02, 03, 04, 05\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.array-buffer should have the right content');
        cb(null);
      });
    }, (t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect --array-length 1 ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBuffer: backingStore=0x[0-9a-f]+, byteLength=5: \\[\n' +
            '  01 ...\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.array-buffer should have the right content with ' +
            '--array-length 1');
        cb(null);
      });
    }]
  },
  // .uint8-array=0x0000393071133e59:<ArrayBufferView: backingStore=0x000000000195b230, byteOffset=0, byteLength=6>,
  // OR
  // .uint8-array=0x000003df9cbe7eb9:<ArrayBufferView [neutered]>,
  'uint8-array': {
    re: new RegExp('.uint8-array=(0x[0-9a-f]+):<ArrayBufferView: ' +
                    'backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
                    'byteLength=6>'),
    desc: '.uint8-array JSArrayBufferView property',
    optional: {
      re: /.uint8-array=0x[0-9a-f]+:<ArrayBufferView \[neutered\]>/,
      reason: 'can be neutered'
    },
    validators: [(t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
            'byteLength=6: \\[\n' +
            '  01, 40, 60, 80, f0, ff\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.uint8-array should have the right content');
        cb(null);
      });
    }, (t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect --array-length 1 ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
            'byteLength=6: \\[\n' +
            '  01 ...\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.uint8-array should have the right content with ' +
            '--array-length 1');
        cb(null);
      });
    }]
  },
  // .buffer=0x000003df9cbe8231:<ArrayBufferView: backingStore=0x0000000002238570, byteOffset=2048, byteLength=6>
  'buffer': {
    re: new RegExp('.buffer=(0x[0-9a-f]+):<ArrayBufferView: ' +
                    'backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
                    'byteLength=6>'),
    desc: '.buffer JSArrayBufferView property',
    validators: [(t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
            'byteLength=6: \\[\n' +
            '  ff, f0, 80, 0f, 01, 00\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.uint8-array should have the right content');
        cb(null);
      });
    }, (t, sess, addresses, name, cb) => {
      const address = addresses[name];
      sess.send(`v8 inspect --array-length 1 ${address}`);

      sess.linesUntil(/\]>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        const re = new RegExp(
            '0x[0-9a-f]+:' +
            '<ArrayBufferView: backingStore=0x[0-9a-f]+, byteOffset=\\d+, ' +
            'byteLength=6: \\[\n' +
            '  ff ...\n' +
            ']>');
        t.ok(re.test(lines),
            'hashmap.buffer should have the right content with ' +
            '--array-length 1');
        cb(null);
      });
    }]
  }
};

const contextTests = {
  'previous': {
    re: /\(previous\)=(0x[0-9a-f]+)[^\n]+/,
    desc: '.(previous)'
  },
  'closure': {
    re: /(\((?:closure|scope_info)\)=0x[0-9a-f]+)[^\n]+/i,
    desc: '.(closure)',
    validator(t, sess, addresses, name, cb) {
      const type = addresses[name].split("=")[0];
      let address = undefined;
      if (type === "(closure)") {
        address = addresses[name].split("=")[1];
      } else if (type === "(scope_info)") {
        address = addresses["previous"];
      } else {
        return cb(new Error("unknown field"));
      }
      sess.send(`v8 inspect ${address}`);
      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(/outerVar[^\n]+"outer variable"/.test(lines),
            'method[[context]].closure.outerVar should exist');
        cb(null);
      });
    }
  },
  'scoped-var': {
    re: /scopedVar[^\n]+"scoped value"/i,
    desc: '.scopedVar',
  },
  'scoped-api': {
    re: /scopedAPI=(0x[0-9a-f]+)[^\n]+Zlib/i,
    desc: '.scopedAPI',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);
      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(/internal fields/.test(lines),
            'method[[context]].closure.scopedAPI should have internal fields');
        cb(null);
      });
    }
  },
  'scoped-array': {
    re: /scopedArray=(0x[0-9a-f]+):<Array: length=2>/i,
    desc: '.scopedArray',
    validator(t, sess, addresses, name, cb) {
      const address = addresses[name];
      sess.send(`v8 inspect ${address}`);
      sess.linesUntil(/}>/, (err, lines) => {
        if (err) return cb(err);
        lines = lines.join('\n');
        t.ok(/\[0\]=<Smi: 0>/.test(lines),
            'method[[context]].scopedArray[0] should be `<Smi: 0>`');
        t.ok(/\[1\]=(0x[0-9a-f]+)[^\n]+Zlib>}>/i.test(lines),
            'method[[context]].scopedArray[1] should be a Zlib');
        cb(null);
      });
    }
  }
}

function verifyBacktrace(t, sess) {
  sess.send('v8 bt');
  sess.wait(/inspect-scenario.js/, (err, line) => {
    if (err) {
      return teardown(t, sess, err);
    }
    let match = line.match(/method\(this=(0x[0-9a-f]+)[^\n]+fn=(0x[0-9a-f]+)/i);
    t.ok(match, 'method should have `this` and `fn`');
    if (!match) {
      return teardown(t, sess);
    }
    verifyMethod(t, sess, match[1], match[2]);
  });
}

function verifyMethod(t, sess, that, fn) {
  const major = parseInt(process.version.match(/v(\d+)\./)[1]);
  // No Context debugging for older node.js
  // FIXME: this may not work if it's testing a prepared core generated by
  // another version of Node.js
  if (major >= 5) {
    verifyMethod(t, sess, that, fn);
  } else {
    verifyMethodSource(t, sess, that, fn);
  }
}

function verifyMethodSource(t, sess, that, fn) {
  sess.send(`v8 inspect -s ${fn}`);
  sess.linesUntil(/^>/, (err, lines) => {
    // We can still continue with this error reported
    t.error(err, `v8 inspect -s <method> should return output`);
    if (err) {
      verifyClass(t, sess, that);
      return;
    }
    // Include 'source:' and '>' to act as boundaries. (Avoid
    // passing if the whole file it displayed instead of just
    // the function we want.)
    const methodSource = '  source:\n' +
    'function method() {\n' +
    '    throw new Error(\'Uncaught\');\n' +
    '  }\n' +
    '>';

    lines = lines.join('\n');
    t.ok(lines.includes(methodSource), 'method source should match');
    verifyClass(t, sess, that);
  });
}

function verifyMethod(t, sess, that, fn) {
  sess.send(`v8 inspect ${fn}`);
  sess.linesUntil(/}>/, (err, lines) => {
    if (err) {
      return teardown(t, sess, err);
    }
    const parent = 'method[[context]]';
    const addresses = collectMembers(
        t, lines.join('\n'), contextTests, parent
    );
    verifyMembers(t, sess, addresses, contextTests, parent, (t, sess) => {
      verifyMethodSource(t, sess, that, fn);
    });
  });
}

function verifyClass(t, sess, that) {
  sess.send(`v8 inspect ${that}`);
  sess.wait(/Class/, (err, line) => {
    if (err) {
      return teardown(t, sess, err);
    }
    t.ok(line.includes(that),
        'address in the detailed view of Class should match the address ' +
        'in the parent\'s view');
    sess.linesUntil(/}>/, (err, lines) => {
      if (err) {
        return teardown(t, sess, err);
      }
      lines = lines.join('\n');
      t.ok(/x=<Smi: 1>/.test(lines),
          'this.x should be <Smi: 1>');
      t.ok(/y=123.456/.test(lines),
          'this.y should be 123.456');
      const match = lines.match(/hashmap=(0x[0-9a-f]+):<Object: Object>/i);
      t.ok(match, 'this.hashmap should be an object');
      if (!match) {
        return teardown(t, sess);
      }
      verifyObject(t, sess, match[1]);
    });
  });
}

function verifyObject(t, sess, hashmap) {
  sess.send(`v8 inspect ${hashmap}`);
  sess.wait(/Object/, (err, line) => {
    if (err) {
      return teardown(t, sess, err);
    }

    t.ok(line.includes(hashmap),
        'address in the detailed view of Object should match the address ' +
        'in the parent\'s view');
    verifyHashMap(t, sess, line, hashmap);
  });
}

function verifyHashMap(t, sess) {
  sess.linesUntil(/}>/, (err, lines) => {
    if (err) {
      return teardown(t, sess, err);
    }
    const parent = 'hashmap';
    const addresses = collectMembers(
        t, lines.join('\n'), hashMapTests, parent);
    verifyMembers(t, sess, addresses, hashMapTests, parent, teardown);
  });
}

function teardown(t, sess, err) {
  t.end(err);
  sess.quit();
}

function collectMembers(t, lines, tests, parent) {
  const addresses = {};
  for (let name of Object.keys(tests)) {
    const test = tests[name];
    const match = lines.match(test.re);
    if (match) {
      t.pass(`${parent}${test.desc} should exist `);
      addresses[name] = match[1];
    } else {
      if (test.optional && test.optional.re.test(lines)) {
        t.skip(`${parent}${test.desc} ${test.optional.reason}`);
      } else {
        t.fail(`${parent}${test.desc} should exist `);
      }
    }
  }
  return addresses;
}

// To avoid lldb output being mixed, we must do the validation one by one
function verifyMembers(t, sess, addresses, tests, parent, next) {
  const arr = Object.keys(addresses);

  function verifyProperty(t, sess, addresses, index, subIndex) {
    if (index >= arr.length) {
      return next(t, sess);
    }

    const name = arr[index];
    const test = tests[name];
    // We are in the middle of in test.validators
    if (subIndex !== undefined) {
      const validators = test.validators;
      if (subIndex >= validators.length) {
        return verifyProperty(t, sess, addresses, index + 1);
      }
      validators[subIndex](t, sess, addresses, name, (err) => {
        t.error(err, `${parent}${test.desc} validator #${subIndex} should complete`);
        verifyProperty(t, sess, addresses, index, subIndex + 1);
      });
      return;
    }

    if (test.validator) { // Single validator
      test.validator(t, sess, addresses, name, (err) => {
        t.error(err, `${parent}${test.desc} validator should complete`);
        verifyProperty(t, sess, addresses, index + 1);
      });
    } else if (test.validators) {  // Multiple validator
      verifyProperty(t, sess, addresses, index, 0);
    } else {  // No validator
      verifyProperty(t, sess, addresses, index + 1);
    }
  }

  // Kickoff
  verifyProperty(t, sess, addresses, 0);
}

tape('v8 inspect', (t) => {
  t.timeoutAfter(15000);

  const sess = common.Session.create('inspect-scenario.js');

  sess.waitBreak((err) => {
    if (err) {
      return teardown(t, sess, err);
    }
    verifyBacktrace(t, sess);
  });
});
