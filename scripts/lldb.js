'use strict';

const child_process = require('child_process');
const path = require('path');
const os = require('os');
const fs = require('fs');

/**
 * @param {string} version lldb version, either in the form '3.9' or '39'
 * @returns {string} Branch of the corresponding lldb release
 */
function versionToBranch(version) {
  return 'release_' + version.replace('.', '');
}

/**
 * @param {string} includeDir Path to the equivalent of llvm-config --includedir
 * @returns {string} Path to the lldb API headers
 */
function getApiHeadersPath(includeDir) {
  return path.join(includeDir, 'lldb', 'API');
}

/**
 * @param {string} includeDir Path to the equivalent of llvm-config --libddir
 * @returns {string} Path to the lldb shared library
 */
function getLibPath(libDir) {
  const lib = os.type() === 'Darwin' ? 'liblldb.dylib' : 'liblldb.so';
  return path.join(libDir, lib);
}

/**
 * Check out source code of the lldb for headers
 * TODO: The llvm project is probably moving to github soon at that point we
 * should stop using the mirror.
 * @param {string} lldbVersion Version of lldb, either like 3.9 or 39
 * @param {string} buildDir Path to the llnode module directory
 * @returns {string} The include directory in the downloaded lldb source code
 */
function cloneHeaders(lldbVersion, buildDir) {
  const lldbHeadersBranch = versionToBranch(lldbVersion);
  const lldbInstallDir = path.resolve(buildDir, `lldb-${lldbVersion}`);

  if (!fs.existsSync(lldbInstallDir)) {
    console.log(`\nCloning lldb ${lldbHeadersBranch} into ${lldbInstallDir}`);
    child_process.execFileSync(
        'git', ['clone',
          '--depth', '1',
          '--branch', lldbHeadersBranch,
          'https://github.com/llvm-mirror/lldb.git',
          lldbInstallDir
        ],
        { stdio: 'inherit' });  // show progress
  } else {
    console.log(`\nSkip cloning lldb headers because ${lldbInstallDir} exists`);
  }
  return path.join(lldbInstallDir, 'include');
}

/**
 * Try to find the first valid executable out of an array executable names.
 * Returns undefined if none of the provided executables is valid, otherwise
 * returns the path to the first found valid executable.
 * @param {string[]} exeNames
 * @returns {string|undefined}
 */
function tryExecutables(exeNames) {
  for (let name of exeNames) {
    let exePath;
    try {
      exePath = child_process.execFileSync(
          'which', [name], { stdio: 'pipe' }  // to suppress stderr
      ).toString().trim();
    } catch (err) {
      // Do nothing - we expect not to find some of these.
    }
    // If the result starts with '/' `which` found a path.
    if (exePath && exePath.startsWith('/')) {
      return exePath;
    }
  }
}

/**
 * Get the lldb version from the lldb executable, exit the process with 1
 * if failed.
 * @param {string} lldbExe
 * @returns {string} Version of the executable in the form like '3.9'
 */
function getLldbVersion(lldbExe) {
  let lldbStr;
  try {
    lldbStr = child_process.execFileSync(lldbExe, ['-v']).toString();
  } catch (err) {
    console.log(err);
    return undefined;
  }
  // Ignore minor revisions like 3.8.1
  const versionMatch = lldbStr.match(/version (\d.\d)/);
  if (versionMatch) {
    return versionMatch[1];
  }

  console.log(`Output from \`${lldbExe} -v\` was ${lldbStr}`);
  return undefined;
}

module.exports = {
  versionToBranch,
  getApiHeadersPath,
  getLibPath,
  cloneHeaders,
  tryExecutables,
  getLldbVersion
};
