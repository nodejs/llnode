'use strict';

const fs = require('fs');
const path = require('path');
const lldb = require('./lldb');

/**
 * Find the lldb to use in the installation of LLVM.
 * @returns {string} Path to the lldb executable or undefined
 */
function getLldbExecutable() {
  if (process.env.npm_config_lldb_exe !== undefined) {
    return process.env.npm_config_lldb_exe;
  }

  const lldbDir = lldb.findWindowsExeDir('lldb.exe');
  if (lldbDir) {
    return path.join(lldbDir, 'lldb.exe');
  }
}

/**
 * Get the directory containing the lldb library.
 * @param {string} lldbExe Path to the corresponding lldb executable
 * @returns {{dir:string, name:string}}
 */
function getLib(lldbExe) {
  const libName = 'liblldb.lib';
  const libDir = path.resolve(path.dirname(lldbExe), '..', 'lib');

  if (!fs.existsSync(path.join(libDir, libName))) {
    return {
      dir: undefined,
      name: libName
    };
  }

  return {
    dir: libDir,
    name: libName
  };
}

/**
 * Get the lldb installation. If prefix is undefined, the headers need to
 * be downloaded.
 * The version string will be in the form like '3.9'
 */
function getLldbInstallation() {
  console.log('Looking for lldb executable...');
  const lldbExe = getLldbExecutable();
  if (!lldbExe) {
    console.log('Could not find any usable lldb executable');
    console.log('Please see the README.md on how to install lldb');
    process.exit(1);
  }
  console.log(`Found lldb executable ${lldbExe}`);

  console.log('\nReading lldb version...');
  const lldbVersion = lldb.getLldbVersion(lldbExe);
  if (!lldbVersion) {
    console.log(`Unable to get the version from the lldb binary ${lldbExe}`);
    process.exit(1);
  }
  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);

  console.log(`\nLooking for shared libraries for lldb ${lldbVersion}...`);
  const lib = getLib(lldbExe);
  if (!lib.dir) {
    console.log(`Could not find ${lib.name} in the same path as the lldb executable`);
    process.exit(1);
  } else {
    console.log(`Found ${lib.name} in ${lib.dir}`);
  }

  return {
    executable: lldbExe,
    version: lldbVersion,
    includeDir: undefined, // Windows binary distribution does not contain headers
    libDir: lib.dir,
    libName: lib.name
  };
}

module.exports = {
  getLldbInstallation
};
