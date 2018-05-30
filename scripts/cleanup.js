'use strict';

const fs = require('fs');
const cwd = process.cwd();
const osName = require('os').type();

let libExt;
if (osName === 'Darwin') {
  libExt = 'dylib';
} else if (osName === 'Windows_NT') {
  libExt = 'dll';
} else {
  libExt = 'so';
}
const llnodeLib = `plugin.${libExt}`;
const destLib = `llnode.${libExt}`;

let buildPath = `${cwd}/build/Release/${llnodeLib}`;

if (!fs.existsSync(buildPath))
  buildPath = `${cwd}/build/Debug/${llnodeLib}`;

const destPath = `${cwd}/${destLib}`;

console.log(`Moving ${buildPath} to ${destPath}`);
fs.renameSync(buildPath, destPath);

console.log(`\nllnode plugin installed, load in lldb with:\n`);
console.log(`(lldb) plugin load ${destPath}`);
console.log('\nor copy plugin to lldb system plugin directory');
console.log('see https://github.com/nodejs/llnode\n');
