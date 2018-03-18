'use strict';

const child_process = require('child_process');
const fs = require('fs');

const linux = require('./linux');
const lldb = require('./lldb');

/**
 * Get the version of the lldb executable,
 * shim this for consistancy in OS naming
 * @param {string} lldbExe
 * @returns {string} lldb version in the form like '39'
 */
function getLldbVersion(lldbExe) {
  // Strip the dots for BSD
  return linux.getLldbVersion(lldbExe).replace('.', '');
}

function printAdvice(version) {
  console.log('The system isn\'t set up correcly.');
  console.log(`Try \`pkg install llvm${version}\``);
  console.log(`And \`ln -s /usr/local/bin/lldb${version} /usr/bin/lldb\``);
}

/**
 * Get the installation directory (prefix) of lldb
 * @param {string} version lldb version for FreeBSD, e.g. '39' for 'lldb-3.9'
 * @returns {string} Directory of the lldb installation
 */
function getInstallDir(version) {
  // Get the directory which should contain the headers and
  // check if they are present.
  // (Using the installed headers will ensure we have the correct ones.)
  console.log('Checking for headers, version is ' + version);
  let installDir;
  try {
    // Notice the executable is named differently from Linux
    installDir = child_process.execFileSync(
        `llvm-config${version}`,
        ['--prefix']
    ).toString().trim();
  } catch (err) {
    // As this is a BSD we know this system is in an improper state
    // So we can exit with an error
    console.log(`Could not execute llvm-config${version}`);
    printAdvice(version);
    console.log(err);
    process.exit(1);
  }

  const headers = lldb.getHeadersPath(installDir);
  if (!fs.existsSync(headers)) {
    // As this is a BSD we know this system is in an improper state
    // So we can exit with an error
    console.log(`Could not find ${headers}`);
    printAdvice(version);
    process.exit(1);
  }

  return installDir;
}

/**
 * Get the lldb installation
 * @returns {{executable: string, version: string, prefix: string}}
 */
function getLldbInstallation() {
  const lldbExe = linux.getLldbExecutable();
  const lldbVersion = getLldbVersion(lldbExe);

  if (lldbVersion === undefined) {
    console.log('Unable to get lldb binary or its version. ' +
                'llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getInstallDir(lldbVersion);
  return {
    executable: lldbExe,
    version: lldbVersion,
    prefix: installedDir
  };
}

module.exports = {
  getLldbInstallation
};
