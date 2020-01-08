'use strict';

const os = require('os');
const path = require('path');
const util = require('util');
const spawn = require('child_process').spawn;
const EventEmitter = require('events').EventEmitter;

exports.fixturesDir = path.join(__dirname, 'fixtures');
exports.projectDir = path.join(__dirname, '..');

exports.core = path.join(os.tmpdir(), 'core');
exports.promptDelay = 200;

function llnodeDebug() {
  // Node v4.x does not support rest
  const args = Array.prototype.slice.call(arguments);
  console.error.apply(console, [`[TEST][${process.pid}]`].concat(args));
}

const debug = exports.debug =
  process.env.TEST_LLNODE_DEBUG ? llnodeDebug : () => { };

let pluginName;
if (process.platform === 'darwin')
  pluginName = 'llnode.dylib';
else if (process.platform === 'win32')
  pluginName = 'llnode.dll';
else
  pluginName = 'llnode.so';

exports.llnodePath = path.join(exports.projectDir, pluginName);
exports.saveCoreTimeout = 360 * 1000;
exports.loadCoreTimeout = 60 * 1000;

let versionMark = /^lldb-|^lldb version/;
exports.versionMark = versionMark;

function SessionOutput(session, stream, timeout) {
  EventEmitter.call(this);
  this.waiting = false;
  this.waitQueue = [];
  let buf = '';
  this.timeout = timeout || 10000;
  this.session = session;

  stream.on('data', (data) => {
    buf += data;

    for (; ;) {
      let index = buf.indexOf('\n');

      if (index === -1)
        break;

      const line = buf.slice(0, index);
      buf = buf.slice(index + 1);

      if (/process \d+ exited/i.test(line))
        session.kill();
      else
        this.emit('line', line);
    }
  });

  // Ignore errors
  stream.on('error', (err) => {
    debug('[stream error]', err);
  });
}
util.inherits(SessionOutput, EventEmitter);

SessionOutput.prototype._queueWait = function _queueWait(retry) {
  if (this.waiting) {
    this.waitQueue.push(retry);
    return false;
  }

  this.waiting = true;
  return true;
};

SessionOutput.prototype._unqueueWait = function _unqueueWait() {
  this.waiting = false;
  if (this.waitQueue.length > 0)
    this.waitQueue.shift()();
};

SessionOutput.prototype.timeoutAfter = function timeoutAfter(timeout) {
  this.timeout = timeout;
};

SessionOutput.prototype.wait = function wait(regexp, callback, allLines) {
  if (!this._queueWait(() => { this.wait(regexp, callback, allLines); }))
    return;

  const self = this;
  const lines = [];

  function onLine(line) {
    lines.push(line);
    if (self.session)
      debug(`[LINE][${self.session.lldb.pid}]`, line);

    if (!regexp.test(line))
      return;

    self.removeListener('line', onLine);
    self._unqueueWait();
    done = true;

    callback(null, allLines ? lines : line);
  }

  let done = false;
  setTimeout(() => {
    if (done)
      return;

    self.removeListener('line', onLine);
    self._unqueueWait();
    console.error(`${'='.repeat(10)} lldb output ${'='.repeat(10)}`);
    console.error(lines.join('\n'));
    console.error('='.repeat(33));
    const message = `Test timeout in ${this.timeout} ` +
      `waiting for ${regexp}`;
    callback(new Error(message));
  }, this.timeout).unref();

  this.on('line', onLine);
};

SessionOutput.prototype.waitBreak = function waitBreak(callback) {
  this.wait(/Process \d+ stopped/i, (err) => {
    if (err)
      return callback(err);

    // Do not resume immediately since the process would print
    // the instructions out and input sent before the stdout finish
    // could be lost
    setTimeout(callback, exports.promptDelay);
  });
};

SessionOutput.prototype.linesUntil = function linesUntil(regexp, callback) {
  this.wait(regexp, callback, true);
};

function Session(options) {
  EventEmitter.call(this);
  const timeout = parseInt(process.env.TEST_TIMEOUT) || 10000;
  const lldbBin = process.env.TEST_LLDB_BINARY || 'lldb';
  const env = Object.assign({}, process.env);

  debug('lldb binary:', lldbBin);
  if (options.scenario) {
    this.needToKill = true;  // need to send 'kill' when quitting
    // lldb -- node scenario.js
    const args = [
      '--',
      process.execPath,
      '--abort_on_uncaught_exception',
      '--expose_externalize_string',
      path.join(exports.fixturesDir, options.scenario)
    ];

    debug('lldb args:', args);
    this.lldb = spawn(lldbBin, args, {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: env
    });
    this.lldb.stdin.write(`plugin load "${exports.llnodePath}"\n`);
    this.lldb.stdin.write('run\n');
  } else if (options.core) {
    this.needToKill = false;  // need to send 'target delete 0' when quitting
    debug('loading core', options.core);
    // lldb node -c core
    this.lldb = spawn(lldbBin, [], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: env
    });
    this.lldb.stdin.write(`plugin load "${exports.llnodePath}"\n`);
    this.lldb.stdin.write(`target create "${options.executable}"` +
      ` --core "${options.core}"\n`);
  }
  this.stdout = new SessionOutput(this, this.lldb.stdout, timeout);
  this.stderr = new SessionOutput(this, this.lldb.stderr, timeout);

  this.stderr.on('line', (line) => {
    debug('[stderr]', line);
  });

  // Map these methods to stdout for compatibility with legacy tests.
  this.wait = SessionOutput.prototype.wait.bind(this.stdout);
  this.waitError = SessionOutput.prototype.wait.bind(this.stderr);
  this.waitBreak = SessionOutput.prototype.waitBreak.bind(this.stdout);
  this.linesUntil = SessionOutput.prototype.linesUntil.bind(this.stdout);
  this.timeoutAfter = SessionOutput.prototype.timeoutAfter.bind(this.stdout);
}
util.inherits(Session, EventEmitter);
exports.Session = Session;

Session.create = function create(scenario) {
  return new Session({ scenario: scenario });
};

function saveCoreLLDB(scenario, core, cb) {
  // Create a core and test
  const sess = Session.create(scenario);
  sess.timeoutAfter(exports.saveCoreTimeout);

  sess.waitBreak(() => {
    sess.send(`process save-core ${core}`);
    // Just a separator
    sess.send('version');
  });

  sess.wait(/lldb-/, (err) => {
    if (err) {
      return cb(err);
    }
    sess.quit();

    cb(null);
  });
}

function spawnWithTimeout(cmd, cb) {
  const proc = spawn(cmd, {
    shell: true,
    stdio: ['pipe', 'pipe', 'pipe']
  });
  const stdout = new SessionOutput(null, proc.stdout, exports.saveCoreTimeout);
  const stderr = new SessionOutput(null, proc.stderr, exports.saveCoreTimeout);
  stdout.on('line', (line) => { debug('[stdout]', line); });
  stderr.on('line', (line) => { debug('[stderr]', line); });

  const timeout = setTimeout(() => {
    console.error(`timeout while saving core dump for scenario "${scenario}"`);
    proc.kill();
  }, exports.saveCoreTimeout);

  proc.on('exit', () => {
    clearTimeout(timeout);
    cb(null);
  });
}

function saveCoreLinux(executable, scenario, core, cb) {
  const cmd = `ulimit -c unlimited && ${executable} ` +
    `--abort_on_uncaught_exception --expose_externalize_string ` +
    `${path.join(exports.fixturesDir, scenario)}; `;
  spawnWithTimeout(cmd, () => {
    // FIXME (mmarchini): Should also handle different core system settings.
    spawnWithTimeout(`mv ./core ${core}`, cb);
  });
}

exports.saveCore = function saveCore(options, cb) {
  const scenario = options.scenario;
  const core = options.core || exports.core;

  if (process.platform === 'linux') {
    saveCoreLinux(process.execPath, scenario, core, cb);
  } else {
    saveCoreLLDB(scenario, core, cb);
  }
}

// Load the core dump with the executable
Session.loadCore = function loadCore(executable, core, cb) {
  const sess = new Session({
    executable: executable,
    core: core
  });

  sess.timeoutAfter(exports.loadCoreTimeout);
  sess.waitCoreLoad(cb);

  return sess;
};

Session.prototype.waitCoreLoad = function waitCoreLoad(callback) {
  this.wait(/Core file[^\n]+was loaded/, callback);
};

Session.prototype.kill = function kill() {
  // if a 'quit' has been sent to lldb, killing it could result in ECONNRESET
  if (this.lldb.channel) {
    debug('kill lldb');
    this.lldb.kill();
    this.lldb = null;
  }
};

Session.prototype.quit = function quit() {
  if (this.needToKill) {
    this.send('kill'); // kill the process launched in lldb
  } else {
    this.send('target delete 0');  // Delete the loaded core dump
  }

  this.send('quit');
};

Session.prototype.send = function send(line, callback) {
  debug(`[SEND][${this.lldb.pid}]`, line);
  this.lldb.stdin.write(line + '\n', callback);
};

Session.prototype.hasSymbol = function hasSymbol(symbol, callback) {
  this.send('target modules dump symtab');
  this.send('version');

  let pattern = new RegExp(symbol);
  this.linesUntil(versionMark, (err, lines) => {
    if (pattern.test(lines.join('\n'))) {
      callback(err, true);
    } else {
      return callback(err, false);
    }
  });
};
