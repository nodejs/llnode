'use strict';

const tape = require('tape');

const common = require('../common');

const sourceCode = [
  "10 function eyecatcher() {",
  "11   crasher();    // # args < # formal parameters inserts an adaptor frame.",
  "12   return this;  // Force definition of |this|.",
  "13 }",
];
const lastLine = new RegExp(sourceCode[sourceCode.length - 1]);

function fatalError(t, sess, err) {
  t.error(err);
  sess.quit();
  return t.end();
}

function testFrameList(t, sess, frameNumber) {
  sess.send(`frame select ${frameNumber}`);
  sess.linesUntil(/frame/, (err) => {
    if (err) {
      return fatalError(t, sess, err);
    }
    sess.send('v8 source list');

    sess.linesUntil(/v8 source list/, () => {
      sess.linesUntil(lastLine, (err, lines) => {
        if (err) {
          return fatalError(t, sess, err);
        }
        t.equal(lines.length, sourceCode.length,
          `v8 source list correct size`);
        for (let i = 0; i < lines.length; i++) {
          t.equal(lines[i].trim(), sourceCode[i], `v8 source list #${i}`);
        }

        sess.send('v8 source list -l 2');

        sess.linesUntil(/v8 source list/, () => {
          sess.linesUntil(lastLine, (err, lines) => {
            if (err) {
              return fatalError(t, sess, err);
            }
            t.equal(lines.length, sourceCode.length - 1,
              `v8 source list -l 2 correct size`);
            for (let i = 0; i < lines.length; i++) {
              t.equal(lines[i].trim(), sourceCode[i + 1],
                `v8 source list -l 2 #${i}`);
            }

            sess.quit();
            t.end();
          });
        });
      });
    });
  });
}

tape('v8 stack', (t) => {
  t.timeoutAfter(15000);

  const sess = common.Session.create('frame-scenario.js');
  sess.waitBreak((err) => {
    t.error(err);
    sess.send('v8 bt');
  });

  sess.linesUntil(/eyecatcher/, (err, lines) => {
    t.error(err);
    lines.reverse();
    t.ok(lines.length > 4, 'frame count');
    // FIXME(bnoordhuis) This can fail with versions of lldb that don't
    // support the GetMemoryRegions() API; llnode won't be able to identify
    // V8 builtins stack frames, it just prints them as anonymous frames.
    lines = lines.filter((s) => !/<builtin>|<stub>/.test(s));
    const eyecatcher = lines[0];
    const adapter = lines[1];
    const crasher = lines[2];
    const exit = lines[3];
    t.ok(/eyecatcher/.test(eyecatcher), 'eyecatcher frame');
    t.ok(/<adaptor>/.test(adapter), 'arguments adapter frame');
    t.ok(/crasher/.test(crasher), 'crasher frame');
    {
      // V8 4.5 does not use EXIT frames, only INTERNAL frames.
      const isv4 = /^v4\./.test(process.version);
      const re = isv4 ? /<internal code>/ : /<exit>/;
      t.ok(re.test(exit), 'exit frame');
    }
    // eyecatcher() is a sloppy mode function that should have an implicit
    // |this| that is the global object.  crasher() is a strict mode function
    // that should have a |this| that is the |undefined| value.
    //
    // Interestingly, V8 4.5 has a quirk where the |this| value is |undefined|
    // in both strict and sloppy mode unless the function actually uses |this|.
    // The test adds unreachable `return this` statements as a workaround.
    t.ok(/this=(0x[0-9a-f]+):<Global proxy>/.test(eyecatcher), 'global this');
    t.ok(/this=(0x[0-9a-f]+):<undefined>/.test(crasher), 'undefined this');

    const eyecatcherFrame = eyecatcher.match(/frame #([0-9]+)/)[1];
    if (!eyecatcherFrame) {
      fatalError(t, sess, "Couldn't determine eyecather's frame number");
    }

    testFrameList(t, sess, eyecatcherFrame);

  });
});
