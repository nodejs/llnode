
'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const lldb = require('./lldb');

/**
 * Try to find an executable llvm-config in the system. Returns undefined
 * it it could not be found.
 * @returns {string|undefined}
 */
function getLlvmConfig() {
  const llvmConfig = lldb.tryExecutables(['llvm-config']);
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
 * On Mac the lldb version string doesn't match the original lldb versions,
 * we need to get it either from `llvm-config --version` (custom installation)
 * or `xcodebuild -version` (Xcode installation).
 *
 * @param {string|undefined} llvmConfig Path to llvm-config, if it's installed
 * @returns {string|undefined} Deduced version of lldb, undefined if failed
 */
function getLldbVersion(llvmConfig) {
  if (llvmConfig) {
    const versionFromConfig = child_process.execFileSync(
        llvmConfig, ['--version']
    ).toString().trim();
    const result = versionFromConfig.split('.').slice(0, 2).join('.');
    console.log(`Retrieved lldb version ${result} ` +
                'from `llvm-config --version`');
    return result;
  }

  let xcodeStr;
  try {
    xcodeStr = child_process.execFileSync(
        'xcodebuild', ['-version']
    ).toString().trim();
  } catch (err) {
    console.log(err);
    console.log('Could not retrieve Xcode version from `xcodebuild -version`');
    return undefined;
  }

  let xcodeVersion;
  let splitStr = xcodeStr.split('\n');
  for (let str of splitStr) {
    if (str.includes('Xcode')) {
      xcodeVersion = str.split(' ')[1];
      break;
    }
  }

  if (xcodeVersion === undefined) {
    console.log(`Could not get Xcode version from:\n${xcodeStr}`);
    return undefined;
  }

  let result;
  let version = parseFloat(xcodeVersion);
  if (version >= 8.3) {
    result = '3.9';
  } else if (version > 8.0) {
    result = '3.8';
  } else {
    result = '3.4';
  }

  if (result !== undefined) {
    console.log('Deduced lldb version from Xcode version: ' +
                `Xcode ${xcodeVersion} -> lldb ${result}`);
  } else {
    console.log('Could not deduce lldb version from Xcode version' +
                xcodeVersion);
  }

  return result;
}

/**
 * Get the directory containing the lldb headers. If it returns undefined,
 * we need to download the headers to ./lldb/include/lldb
 * Using the installed headers will ensure we have the correct ones.
 * @param {string|undefined} llvmConfig Path to llvm-config, if it's installed
 * @returns {string|undefined} the lldb include directory, undefined if failed
 */
function getIncludeDir(llvmConfig) {
  if (!llvmConfig) {
    return undefined;
  }
  const includeDir = child_process.execFileSync(
      llvmConfig, ['--includedir']
  ).toString().trim();
  if (fs.existsSync(lldb.getApiHeadersPath(includeDir))) {
    return includeDir;
  }
}

/**
 * Get the directory containing the lldb shared libraries. If it returns
 * undefined, the shared library will be searched from the global search paths
 * @param {string|undefined} llvmConfig Path to llvm-config, if it's installed
 * @returns {string|undefined} the lldb library directory, undefined if failed
 */
function getLibDir(llvmConfig) {
  if (!llvmConfig) {
    return undefined;
  }
  const libDir = child_process.execFileSync(
      llvmConfig, ['--libdir']
  ).toString().trim();
  if (fs.existsSync(path.join(libDir, 'liblldb.dylib'))) {
    return libDir;
  }
}

/**
 * Get the lldb installation.
 * The version string will be in the form like '3.9'
 */
function getLldbInstallation() {
  const lldbExe = process.env.npm_config_lldb_exe || 'lldb';
  // We cannot just use the executable specified with
  // process.env.npm_config_lldb_exe to determine the version of lldb
  // because we do not know how. We can only use llvm-config or xcodebuild
  // to retrieve the version.
  console.log(`Using lldb executable ${lldbExe}`);

  console.log('\nLooking for llvm-config...');
  const llvmConfig = getLlvmConfig();
  if (!llvmConfig) {
    console.log('No llvm-config found');
  } else {
    console.log(`Using llvm-config in ${llvmConfig}`);
  }

  console.log('\nReading lldb version...');
  const lldbVersion = getLldbVersion(llvmConfig);
  if (!lldbVersion) {
    console.log('Unable to deduce the version of lldb, ' +
                'llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  console.log(`\nLooking for headers for lldb ${lldbVersion}...`);
  const includeDir = getIncludeDir(llvmConfig);
  if (!includeDir) {
    console.log('Could not find the headers, will download them later');
  } else {
    console.log(`Found lldb headers in ${includeDir}`);
  }

  console.log(`\nLooking for shared libraries for lldb ${lldbVersion}...`);
  const libDir = getLibDir(llvmConfig);
  if (!libDir) {
    console.log('Could not find the shared libraries ');
    console.log('llnode will be linked to the LLDB shared framework from ' +
                'the Xcode installation');
  } else {
    console.log(`Found lldb liblldb.dylib in ${libDir}`);
  }

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
