'use strict';

const path = require('path');
const util = require('util');
const spawn = require('child_process').spawn;
const EventEmitter = require('events').EventEmitter;

exports.fixturesDir = path.join(__dirname, 'fixtures');
exports.buildDir = path.join(__dirname, '..', 'out', 'Release');

let pluginName;
if (process.platform === 'darwin')
  pluginName = 'llnode.dylib';
else if (process.platform === 'windows')
  pluginName = 'llnode.dll';
else
  pluginName = path.join('lib.target', 'llnode.so');

exports.llnodePath = path.join(exports.buildDir, pluginName);

function Session(scenario) {
  EventEmitter.call(this);

  // lldb -- node scenario.js
  this.lldb = spawn(process.env.TEST_LLDB_BINARY || 'lldb', [
    '--',
    process.execPath,
    '--abort_on_uncaught_exception',
    path.join(exports.fixturesDir, scenario)
  ], {
    stdio: [ 'pipe', 'pipe', 'inherit' ]
  });

  this.lldb.stdin.write(`plugin load "${exports.llnodePath}"\n`);
  this.lldb.stdin.write('run\n');

  this.initialized = false;
  this.waiting = false;
  this.waitQueue = [];

  let buf = '';
  this.lldb.stdout.on('data', (data) => {
    buf += data;

    for (;;) {
      let index = buf.indexOf('\n');
      if (index === -1)
        break;

      const line = buf.slice(0, index);
      buf = buf.slice(index + 1);

      if (/process \d+ exited/i.test(line))
        this.kill();
      else if (this.initialized)
        this.emit('line', line);
      else if (/process \d+ launched/i.test(line))
        this.initialized = true;
    }
  });

  // Ignore errors
  this.lldb.stdin.on('error', () => {});
  this.lldb.stdout.on('error', () => {});
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

Session.prototype._queueWait = function _queueWait(retry) {
  if (this.waiting) {
    this.waitQueue.push(retry);
    return false;
  }

  this.waiting = true;
  return true;
};

Session.prototype._unqueueWait = function _unqueueWait() {
  this.waiting = false;
  if (this.waitQueue.length > 0)
    this.waitQueue.shift()();
};

Session.prototype.wait = function wait(regexp, callback) {
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

Session.prototype.waitBreak = function waitBreak(callback) {
  this.wait(/Process \d+ stopped/i, callback);
};

Session.prototype.linesUntil = function linesUntil(regexp, callback) {
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
