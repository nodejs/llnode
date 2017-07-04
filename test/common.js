'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const util = require('util');
const spawn = require('child_process').spawn;
const EventEmitter = require('events').EventEmitter;

exports.fixturesDir = path.join(__dirname, 'fixtures');
exports.buildDir = path.join(__dirname, '..', 'out', 'Release');

exports.core = path.join(os.tmpdir(), 'core');
exports.ranges = exports.core + '.ranges';

let pluginName;
if (process.platform === 'darwin') {
  pluginName = 'llnode.dylib';
} else if (process.platform === 'windows') {
  pluginName = 'llnode.dll';
} else {
  pluginName = 'llnode.so';
}

// Check we have a plugin library and whether llnode was build with
// npm install or ./gyp_llnode.
// If we have both versions or neither that's an error.
const npm_path = pluginName;
const gyp_path = path.join(exports.buildDir, pluginName);
const npm_exists = fs.existsSync(npm_path);
const gyp_exists = fs.existsSync(gyp_path);

if (npm_exists && gyp_exists) {
  console.error(`${pluginName} has been built with both npm install and` +
    ' gyp_llnode.');
  console.error('Please remove one version before running npm test.');
  process.exit(1);
}

if (!npm_exists && !gyp_exists) {
  console.error(`${pluginName} has not been built.`);
  console.error('Please run `npm install` or `../gyp_llnode && make -C out/`');
  process.exit(1);
}

exports.llnodePath = npm_exists ? npm_path : gyp_path;

function SessionOutput(session, stream) {
  EventEmitter.call(this);
  this.waiting = false;
  this.waitQueue = [];

  let buf = '';

  stream.on('data', (data) => {
    buf += data;

    for (;;) {
      let index = buf.indexOf('\n');

      if (index === -1)
        break;

      const line = buf.slice(0, index);
      buf = buf.slice(index + 1);

      if (/process \d+ exited/i.test(line))
        session.kill();
      else if (session.initialized)
        this.emit('line', line);
      else if (/process \d+ launched/i.test(line))
        session.initialized = true;
    }
  });

  // Ignore errors
  stream.on('error', () => {});
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

SessionOutput.prototype.wait = function wait(regexp, callback) {
  if (!this._queueWait(() => { this.wait(regexp, callback); }))
    return;

  const self = this;
  this.on('line', function onLine(line) {
    if (!regexp.test(line))
      return;

    self.removeListener('line', onLine);
    self._unqueueWait();

    callback(line);
  });
};

SessionOutput.prototype.waitBreak = function waitBreak(callback) {
  this.wait(/Process \d+ stopped/i, callback);
};

SessionOutput.prototype.linesUntil = function linesUntil(regexp, callback) {
  if (!this._queueWait(() => { this.linesUntil(regexp, callback); }))
    return;

  const lines = [];
  const self = this;
  this.on('line', function onLine(line) {
    lines.push(line);

    if (!regexp.test(line))
      return;

    self.removeListener('line', onLine);
    self._unqueueWait();

    callback(lines);
  });
};


function Session(scenario) {
  EventEmitter.call(this);

  // lldb -- node scenario.js
  this.lldb = spawn(process.env.TEST_LLDB_BINARY || 'lldb', [
    '--',
    process.execPath,
    '--abort_on_uncaught_exception',
    path.join(exports.fixturesDir, scenario)
  ], {
    stdio: [ 'pipe', 'pipe', 'pipe' ],
    env: util._extend(util._extend({}, process.env), {
      LLNODE_RANGESFILE: exports.ranges
    })
  });

  this.lldb.stdin.write(`plugin load "${exports.llnodePath}"\n`);
  this.lldb.stdin.write('run\n');

  this.initialized = false;
  this.stdout = new SessionOutput(this, this.lldb.stdout);
  this.stderr = new SessionOutput(this, this.lldb.stderr);

  // Map these methods to stdout for compatibility with legacy tests.
  this.wait = SessionOutput.prototype.wait.bind(this.stdout);
  this.waitBreak = SessionOutput.prototype.waitBreak.bind(this.stdout);
  this.linesUntil = SessionOutput.prototype.linesUntil.bind(this.stdout);
}
util.inherits(Session, EventEmitter);
exports.Session = Session;

Session.create = function create(scenario) {
  return new Session(scenario);
};

Session.prototype.kill = function kill() {
  this.lldb.kill();
  this.lldb = null;
};

Session.prototype.quit = function quit() {
  this.send('kill');
  this.send('quit');
};

Session.prototype.send = function send(line, callback) {
  this.lldb.stdin.write(line + '\n', callback);
};


exports.generateRanges = function generateRanges(cb) {
  let script;
  if (process.platform === 'darwin')
    script = path.join(__dirname, '..', 'scripts', 'otool2segments.py');
  else
    script = path.join(__dirname, '..', 'scripts', 'readelf2segments.py');

  const proc = spawn(script, [ exports.core ], {
    stdio: [ null, 'pipe', 'inherit' ]
  });

  proc.stdout.pipe(fs.createWriteStream(exports.ranges));

  proc.on('exit', (status) => {
    cb(status === 0 ? null : new Error('Failed to generate ranges'));
  });
};
