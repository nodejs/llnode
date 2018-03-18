'use strcit';

const child_process = require('child_process');
const path = require('path');
const fs = require('fs');

/**
 * @param {string} version lldb version, either in the form '3.9' or '39'
 * @returns {string} Branch of the corresponding lldb release
 */
function versionToBranch(version) {
  return 'release_' + version.replace('.', '');
}

/**
 * Equivalent to `llvm-config --includedir`/lldb
 * @param {string} lldbInstallDir Path to the lldb installation
 * @returns {string} Path to the lldb headers
 */
function getHeadersPath(lldbInstallDir) {
  return path.join(lldbInstallDir, 'include', 'lldb');
}

/**
 * Check out source code of the lldb for headers
 * TODO: The llvm project is probably moving to github soon at that point we
 * should stop using the mirror.
 * @param {string} lldbVersion Version of lldb, either like 3.9 or 39
 * @param {string} buildDir
 * @returns {string} The directory where the source code is checked out
 */
function cloneHeaders(lldbVersion, buildDir) {
  const lldbHeadersBranch = versionToBranch(lldbVersion);
  const lldbInstallDir = `lldb-${lldbVersion}`;

  if (!fs.existsSync(path.join(buildDir, lldbInstallDir))) {
    console.log(`Cloning lldb ${lldbHeadersBranch} into ${lldbInstallDir}`);
    child_process.execFileSync('git',
        ['clone', '--depth=1', '-b', lldbHeadersBranch,
          'https://github.com/llvm-mirror/lldb.git', lldbInstallDir],
        {
          cwd: buildDir,
          stdio: 'inherit'  // show progress
        });
  } else {
    console.log(`Skip cloning lldb headers because ${lldbInstallDir} exists`);
  }
  return lldbInstallDir;
}

module.exports = {
  versionToBranch,
  getHeadersPath,
  cloneHeaders
};
