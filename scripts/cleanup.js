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

const llnodeLib = `llnode.${libExt}`;
const buildType = 'Release';

// Move the library somewhere easy to remember.
// TODO: get build type from somewhere else?

const outputDir = `${cwd}/out/${buildType}`
let buildPath = `${outputDir}/${llnodeLib}`;
const finalPath = `${cwd}/${llnodeLib}`;

if (!fs.existsSync(buildPath)) {
  // Original gyp does not move the result
  buildPath = `${outputDir}/obj.target/${llnodeLib}`;
}

console.log(`Copying ${buildPath} to ${finalPath}`);
child_process.execSync(`mv ${buildPath} ${finalPath}`);

console.log(`${os.EOL}llnode plugin installed, load in lldb with:`);
console.log(`(lldb) plugin load ${finalPath}`);
console.log(`or copy plugin to lldb system plugin directory, see www.npmjs.org/llnode${os.EOL}`);
