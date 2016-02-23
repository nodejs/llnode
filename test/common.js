'use strict';

const path = require('path');
const util = require('util');
const spawn = require('child_process').spawn;
const EventEmitter = require('events').EventEmitter;

exports.addon = require('./addon');

exports.fixturesDir = path.join(__dirname, 'fixtures');
exports.buildDir = path.join(__dirname, '..', 'out', 'Release');

let ext;
if (process.platform === 'darwin')
  ext = '.dylib';
else if (process.platform === 'windows')
  ext = '.dll';
else
  ext = '.so';

exports.llnodePath = path.join(exports.buildDir, 'llnode' + ext);

function Session(scenario) {
  EventEmitter.call(this);

  // lldb -- node scenario.js
  this.lldb = spawn('lldb', [
    '--',
    process.execPath,
    path.join(exports.fixturesDir, scenario)
  ], {
    stdio: [ 'pipe', 'pipe', 'inherit' ]
  });

  this.lldb.stdin.write('b llnode_test_addon::Method\n');
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

Session.prototype.wait = function wait(regexp, callback) {
  if (this.waiting) {
    this.waitQueue.push(() => {
      this.wait(regexp, callback);
    });
    return;
  }

  this.waiting = true;

  const self = this;
  this.on('line', function onLine(line) {
    if (!regexp.test(line))
      return;

    self.removeListener('line', onLine);

    self.waiting = false;
    if (self.waitQueue.length > 0)
      self.waitQueue.shift()();

    callback(line);
  });
};

Session.prototype.waitBreak = function waitBreak(callback) {
  this.wait(/Process \d+ stopped/i, callback);
};
