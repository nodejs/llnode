'use strict';

const child_process = require('child_process');

main();

function main() {
  runFile('node-gyp', ['rebuild']);
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
