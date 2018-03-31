'use strict';

const os = require('os');
const child_process = require('child_process');
const fs = require('fs');

const cwd = process.cwd();
const osName = os.type();
var libExt = 'so';

if (osName === 'Darwin') {
  libExt = 'dylib';
}

const llnodeLib = `plugin.${libExt}`;
const destLib = `llnode.${libExt}`;

let buildPath = `${cwd}/build/Release/${llnodeLib}`;

if (!fs.existsSync(buildPath)) {
  buildPath = `${cwd}/build/Debug/${llnodeLib}`;
}

const destPath = `${cwd}/${destLib}`;

console.log(`Moving ${buildPath} to ${destPath}`);
fs.renameSync(buildPath, destPath);

console.log(`${os.EOL}llnode plugin installed, load in lldb with:`);
console.log(`(lldb) plugin load ${destPath}`);
console.log(`or copy plugin to lldb system plugin directory, see www.npmjs.org/llnode${os.EOL}`);
