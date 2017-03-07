'use strict';

const os = require('os');
const child_process = require('child_process');

const cwd = process.cwd();
const osName = os.type();
var libExt = 'so';

if (osName === 'Darwin') {
  libExt = 'dylib';
}

const llnodeLib = `llnode.${libExt}`;

// Move the library somewhere easy to remember.
console.log(`Copying ${cwd}/out/Release/${llnodeLib} to ${cwd}/${llnodeLib}`);
child_process.execSync(`mv ${cwd}/out/Release/${llnodeLib} ${cwd}/${llnodeLib}`);

console.log(`${os.EOL}llnode plugin installed, load in lldb with:`);
console.log(`(lldb) plugin load ${cwd}/${llnodeLib}`);
console.log(`or copy plugin to lldb system plugin directory, see www.npmjs.org/llnode${os.EOL}`);
