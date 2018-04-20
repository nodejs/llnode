'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');

const lldb = require('./lldb');

function printAdvice(version) {
  console.log('The system is not set up correctly. Please try:\n');
  console.log(`$ pkg install llvm${version}`);
  console.log(`$ ln -s /usr/local/bin/lldb${version} /usr/bin/lldb`);
}

/**
 * Find the 'best' lldb to use, exit the process with 1 if failed.
 * The search happens in the following order:
 * - the one specified by the user using npm --lldb_exe=... install llnode
 * - the default lldb executable (`lldb`)
 * - the higest known lldb version (`lldb-xy`)
 * - the names of future releases are predictable for FreeBSD
 *
 * @returns {string} Path to the lldb executable
 */
function getLldbExecutable() {
  if (process.env.npm_config_lldb_exe !== undefined) {
    return process.env.npm_config_lldb_exe;
  }

  const lldbExeNames = [
    'lldb', 'lldb50', 'lldb40',
    'lldb39', 'lldb38', 'lldb37', 'lldb36'
  ];

  const lldbExe = lldb.tryExecutables(lldbExeNames);
  return lldbExe;
}

/**
 * Get the version of the lldb executable, shim this for consistency
 * in OS naming
 * @param {string} lldbExe Path to lldb executable
 * @returns {string} lldb version in the form like '39'
 */
function getLldbVersion(lldbExe) {
  // Strip the dots for BSD
  return lldb.getLldbVersion(lldbExe).replace('.', '');
}

/**
 * Try to find an executable llvm-config in the system. Returns undefined
 * it it could not be found.
 * @param {string} version lldb version for FreeBSD, e.g. '39' for lldb-3.9
 * @returns {string|undefined}
 */
function getLlvmConfig(version) {
  let llvmConfig = lldb.tryExecutables([`llvm-config${version}`]);
  if (!llvmConfig) {
    llvmConfig = lldb.tryExecutables(['llvm-config']);
  }
  if (llvmConfig) {
    try {
      child_process.execFileSync(llvmConfig, ['--version']);
    } catch (err) {
      return undefined;  // Not really executable
    }
  }

  return llvmConfig;
}

/**
 * Get the directory containing the lldb headers.
 * Using the installed headers will ensure we have the correct ones.
 * @param {string} llvmConfig Path to llvm-config
 * @returns {string} the lldb include directory
 */
function getIncludeDir(llvmConfig) {
  const includeDir = child_process.execFileSync(
      llvmConfig, ['--includedir']
  ).toString().trim();
  const headers = lldb.getApiHeadersPath(includeDir);
  if (fs.existsSync(headers)) {
    return includeDir;
  }
  return undefined;
}

/**
 * Get the directory containing the lldb shared libraries.
 * @param {string} llvmConfig Path to llvm-config
 * @returns {string} the lldb library directory
 */
function getLibDir(llvmConfig) {
  const libDir = child_process.execFileSync(
      llvmConfig, ['--libdir']
  ).toString().trim();
  if (fs.existsSync(path.join(libDir, 'liblldb.so'))) {
    return libDir;
  }
  return undefined;
}

/**
 * Get the lldb installation. On FreeBSD we expect lldb to be
 * installed with the llvm package, which comes with headers,
 * shared libraries, and llvm-config.
 */
function getLldbInstallation() {
  console.log('Looking for lldb executable...');
  const lldbExe = getLldbExecutable();
  if (!lldbExe) {
    printAdvice('50');  // Recommended version is lldb 5.0
    process.exit(1);
  }
  console.log(`Found lldb executable ${lldbExe}`);

  console.log('\nReading lldb version...');
  const lldbVersion = getLldbVersion(lldbExe);
  if (!lldbVersion === undefined) {
    console.log(`Unable to get the version from the lldb binary ${lldbExe}`);
    process.exit(1);
  }
  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);

  console.log(`\nLooking for llvm-config for lldb ${lldbVersion}...`);
  const llvmConfig = getLlvmConfig(lldbVersion);
  if (!llvmConfig) {
    console.log(`Could not find llvm-config in the system`);
    printAdvice(lldbVersion);
    process.exit(1);
  }
  console.log(`Using llvm-config in ${llvmConfig}`);

  console.log(`\nLooking for headers for lldb ${lldbVersion}...`);
  const includeDir = getIncludeDir(llvmConfig);
  if (!includeDir) {
    console.log(`Could not find lldb headers in the system`);
    printAdvice(lldbVersion);
    process.exit(1);
  }
  console.log(`Found lldb headers in ${includeDir}`);

  console.log(`\nLooking for shared libraries for lldb ${lldbVersion}...`);
  const libDir = getLibDir(llvmConfig);
  if (!libDir) {
    console.log(`Could not find shared libraries in the system`);
    printAdvice(lldbVersion);
    process.exit(1);
  }
  console.log(`Found lldb liblldb.so in ${libDir}`);

  return {
    executable: lldbExe,
    version: lldbVersion,
    includeDir: includeDir,
    libDir: libDir
  };
}

module.exports = {
  getLldbInstallation
};
