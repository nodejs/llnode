'use strict';

const child_process = require('child_process');
const os = require('os');

const lldb = require('./lldb');

main();

function main() {
  const osName = os.type();
  if (osName === 'Windows_NT') {
    return mainWindows();
  }

  runFile('node-gyp', ['rebuild']);
}

function mainWindows() {
  const clangExe = 'clang-cl.exe';
  const clangDir = lldb.findWindowsExeDir(clangExe);

  if (!clangDir) {
    console.log(`Could not find ${clangExe}`);
    process.exit(1);
  }
  console.log(`Using ${clangExe} found at ${clangDir}`);

  runShell('node-gyp clean');
  runShell('node-gyp configure');
  runShell(`node-gyp build /p:CLToolExe="${clangExe}" /p:CLToolPath="${clangDir}"`);
}

/**
 * execFileSync wrapper, exits on error.
 * @param {string} file Executable to run
 * @param {string[]} args List of arguments
 */
function runFile(file, args) {
  try {
    child_process.execFileSync(file, args, { stdio: 'inherit' });
  } catch (err) {
    process.exit(err.status);
  }
}

/**
 * execSync wrapper, exits on error.
 * @param {string} command Command to run
 */
function runShell(command) {
  try {
    child_process.execSync(command, { stdio: 'inherit' });
  } catch (err) {
    process.exit(err.status);
  }
}
