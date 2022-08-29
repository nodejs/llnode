'use strict';

const { promisify } = require('util');

const tape = require('tape');

const common = require('../common');
const { nodejsVersion } = common;

const sourceCodes = {
  "fnFunctionName": [
    "26 function fnFunctionName() {",
    "27   (new Module()).fnInferredNamePrototype();",
    "28   return this;  // Force definition of |this|.",
    "29 }",
  ],
  "fnInferredNamePrototype": [
    "22 Module.prototype.fnInferredNamePrototype = function() {",
    "23   fnInferredName();",
    "24 }",
    "25",
  ],
  "fnInferredName": [
    "14 fnInferredName = (() => function () {",
    "15   crasher();    // # args < # formal parameters inserts an adaptor frame.",
    "16 })();",
    "17",
  ],
};

function fatalError(t, sess, err) {
  t.error(err);
  sess.quit();
  return t.end();
}

async function testFrameList(t, sess, frameNumber, sourceCode, cb) {
  const lastLine = new RegExp(sourceCode[sourceCode.length - 1]);

  sess.send(`frame select ${frameNumber}`);
  await sess.linesUntil(/frame/);
  sess.send('v8 source list');
  await sess.linesUntil(/v8 source list/);

  let lines = await sess.linesUntil(lastLine);
  t.equal(lines.length, sourceCode.length,
          `v8 source list correct size`);
  for (let i = 0; i < lines.length; i++) {
    t.equal(lines[i].trim(), sourceCode[i], `v8 source list #${i}`);
  }

  sess.send('v8 source list -l 2');
  await sess.linesUntil(/v8 source list/);
  lines = await sess.linesUntil(lastLine);

  t.equal(lines.length, sourceCode.length - 1,
          `v8 source list -l 2 correct size`);
  for (let i = 0; i < lines.length; i++) {
    t.equal(lines[i].trim(), sourceCode[i + 1],
            `v8 source list -l 2 #${i}`);
  }
}

tape('v8 stack', async (t) => {
  t.timeoutAfter(30000);

  const sess = common.Session.create('frame-scenario.js');
  sess.waitBreak = promisify(sess.waitBreak);
  sess.linesUntil = promisify(sess.linesUntil);

  try {
    await sess.waitBreak();
    sess.send('v8 bt');

    let lines = await sess.linesUntil(/\sfnFunctionName\(/);

    lines.reverse();
    t.ok(lines.length > 4, 'frame count');

    lines = lines.filter((s) => !/<builtin>|<stub>/.test(s));
    const hasArgumentAdaptorFrame = nodejsVersion()[0] < 16;
    const argumentAdaptorOffset = hasArgumentAdaptorFrame ? 1 : 0;
    const exit = lines[4 + argumentAdaptorOffset];
    const crasher = lines[3 + argumentAdaptorOffset];
    if (hasArgumentAdaptorFrame) {
      const adaptor = lines[3];
      t.ok(/<adaptor>/.test(adaptor), 'arguments adapter frame');
    }
    const fnInferredName = lines[2];
    const fnInferredNamePrototype = lines[1];
    const fnFunctionName = lines[0];
    t.ok(/<exit>/.test(exit), 'exit frame');
    t.ok(/crasher/.test(crasher), 'crasher frame');
    if (nodejsVersion()[0] < 12)
      t.ok(/\sfnInferredName\(/.test(fnInferredName), 'fnInferredName frame');
    t.ok(/\sModule.fnInferredNamePrototype\(/.test(fnInferredNamePrototype),
         'fnInferredNamePrototype frame');
    t.ok(/\sfnFunctionName\(/.test(fnFunctionName), 'fnFunctionName frame');
    // fn() is a sloppy mode function that should have an implicit
    // |this| that is the global object.  crasher() is a strict mode function
    // that should have a |this| that is the |undefined| value.
    //
    // Interestingly, V8 4.5 has a quirk where the |this| value is |undefined|
    // in both strict and sloppy mode unless the function actually uses |this|.
    // The test adds unreachable `return this` statements as a workaround.
    t.ok(/this=(0x[0-9a-f]+):<Global proxy>/.test(fnFunctionName),
         'global this');
    t.ok(/this=(0x[0-9a-f]+):<undefined>/.test(crasher), 'undefined this');

    // TODO(kvakil): This doesn't work on Node 16 for some reason. Skipping for
    // now.
    if (nodejsVersion()[0] == 16) {
      t.skip('tests for printing function source code');
      sess.quit();
      return t.end();
    }

    // TODO(mmarchini): also test positional info (line, column)

    const fnFunctionNameFrame = fnFunctionName.match(/frame #([0-9]+)/)[1];
    if (fnFunctionNameFrame) {
      await testFrameList(t, sess, fnFunctionNameFrame,
                          sourceCodes['fnFunctionName']);
    } else {
      fatalError(t, sess, "Couldn't determine fnFunctionName's frame number");
    }

    if (nodejsVersion()[0] < 12) {
      const fnInferredNamePrototypeFrame =
        fnInferredNamePrototype.match(/frame #([0-9]+)/)[1];
      if (fnInferredNamePrototypeFrame) {
        await testFrameList(t, sess, fnInferredNamePrototypeFrame,
                            sourceCodes['fnInferredNamePrototype']);
      } else {
        fatalError(t, sess,
                  "Couldn't determine fnInferredNamePrototype's frame number");
      }
    }

    const fnInferredNameFrame = fnInferredName.match(/frame #([0-9]+)/)[1];
    if (fnInferredNameFrame) {
      await testFrameList(t, sess,
                          fnInferredNameFrame, sourceCodes['fnInferredName']);
    } else {
      fatalError(t, sess, "Couldn't determine fnInferredName's frame number");
    }

    sess.quit();
    return t.end();
  } catch (err) {
    fatalError(t, sess, err);
  }
});
