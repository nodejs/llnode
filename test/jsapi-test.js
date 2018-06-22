'use strict';

const fromCoredump = require('../').fromCoredump;

const debug = process.env.TEST_LLNODE_DEBUG ?
  console.log.bind(console) : () => { };

const common = require('./common');
const tape = require('tape');

tape('llnode API', (t) => {
  t.timeoutAfter(common.saveCoreTimeout);

  // Use prepared core and executable to test
  if (process.env.LLNODE_CORE && process.env.LLNODE_NODE_EXE) {
    test(process.env.LLNODE_NODE_EXE, process.env.LLNODE_CORE, t);
    t.end();
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
  debug('============= Loading ==============');
  // Equivalent to lldb executable -c core
  debug(`Loading core dump: ${core}, executable: ${executable}`);
  const llnode = fromCoredump(core, executable);

  verifySBProcess(llnode, t);
  const typeMap = verifyBasicTypes(llnode, t);
  const processType = verifyProcessType(typeMap, llnode, t);
  verifyProcessInstances(processType, llnode, t);
}

function verifySBProcess(llnode, t) {
  const processInfo = llnode.getProcessInfo();
  debug('Process info', processInfo);
  const procRe = new RegExp(
    'SBProcess: pid = (\\d+), state = (\\w+), ' +
    'threads = (\\d+), executable = .+');
  const procMatch = processInfo.match(procRe);
  t.ok(procMatch, 'SBProcess info should be formatted correctly');

  const procObj = llnode.getProcessObject();
  debug('Process object', procObj);
  t.equal(procObj.pid, parseInt(procMatch[1]), 'SBProcess pid');
  t.equal(procObj.state, procMatch[2], 'SBProcess state');
  t.equal(procObj.threadCount, parseInt(procMatch[3]),
    'SBProcess thread count');
  t.ok(procObj.threadCount > 0,
    'SBProcess should have more than one thread');
  t.ok(Array.isArray(procObj.threads),
    'processObject.threads should be an array');
  t.equal(procObj.threads.length,
    procObj.threadCount,
    'processObject.threads should contain all the threads');

  let i = 0;
  for (const thread of procObj.threads) {
    debug(`Thread ${i}:`, thread);
    t.equal(thread.threadId, i++, 'thread.threadId');
    t.ok(Array.isArray(thread.frames),
      'thread.frames should be an array');
    t.equal(thread.frameCount, thread.frames.length,
      'thread.frames should contain all the frames');

    for (const frame of thread.frames) {
      debug(`  #`, frame);
      t.ok(typeof frame.function === 'string',
        'frame.function should be a string');
    }
  }
}

function verifyBasicTypes(llnode, t) {
  debug('============= Heap Types ==============');
  const heapTypes = llnode.getHeapTypes();
  // debug('Heap types', heapTypes);
  const basicTypes = [
    // basic JS types
    '(Array)', '(String)', 'Object', '(Object)', '(ArrayBufferView)',
    // Node types
    'process', 'NativeModule', 'console', 'TickObject'
  ].sort();

  const typeMap = new Map();
  for (const item of heapTypes) {
    if (basicTypes.indexOf(item.typeName) !== -1) {
      typeMap.set(item.typeName, item);
    }
  }

  const foundTypes = Array.from(typeMap.keys()).sort();
  t.deepEqual(foundTypes, basicTypes,
    'The heap should contain all the basic types');

  return typeMap;
}

function verifyProcessType(typeMap, llnode, t) {
  const processType = typeMap.get('process');

  t.equal(processType.typeName, 'process',
    'The typeName of process type should be "process"')
  t.ok(processType.instanceCount > 0,
    'There should be more than one process instances');
  t.ok(processType.totalSize > 0,
    'The process objects should have a non-zero size');
  return processType;
}

function verifyProcessInstances(processType, llnode, t) {
  // TODO: should be implemented as an iterator
  let foundProcess = false;

  const propRe = [
    /.pid=<Smi: \d+>/,
    /.platform=0x[0-9a-z]+:<String: ".+">/,
    /.arch=0x[0-9a-z]+:<String: ".+">/,
    /.version=0x[0-9a-z]+:<String: ".+">/,
    /.versions=0x[0-9a-z]+:<Object: Object>/,
    /.release=0x[0-9a-z]+:<Object: Object>/,
    /.execPath=0x[0-9a-z]+:<String: ".+">/,
    /.execArgv=0x[0-9a-z]+:<Array: length=\d+>/,
    /.argv=0x[0-9a-z]+:<Array: length=\d+>/
  ];

  const visited = new Map();

  for (const instance of processType.instances) {
    t.ok(!visited.get(instance.address),
      'should not duplicate instances');
    visited.set(instance.address, instance.value);
    t.deepEqual(
      instance,
      llnode.getObjectAtAddress(instance.address),
      'instance should be the same as obtained from address')

    if (propRe.every((re) => re.test(instance.value))) {
      foundProcess = true;
    }
  }
  t.ok(foundProcess, 'should find the process object');
}
