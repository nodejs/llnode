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
  // `llvm-project` v1-3 branches are in the form `release/major.minor.x`:
  //   release/1.0.x ... release/3.9.x
  // `llvm-project` v4-latest branches are in the form `release/major.x`:
  //   release/4.x ... release/12.x

  // split into array of semver components
  var chars = (version.indexOf('.') === -1
    ? version.split('')                         // split `39` into ['3', '9']
    : version.split('.').filter(x => x !== '.') // split `3.9` into ['3', '9']
  );

  // if version < 4, keep `major.minor` components
  // if version >= 4, only keep `major` component
  chars = chars.slice(0, (+chars[0] >= 4) ? 1 : 2);

  // join components into the form `release/3.9.x`
  return 'release/' + chars.concat('x').join('.');
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
 * Check if the installed version of git supports sparse checkout. Notably,
 * Ubuntu 18.04 (which is not EOL at time of writing) has git 2.17, which does
 * not support sparse-checkout.
 */
function doesGitSupportSparseCheckout() {
  const returnCode = child_process.spawnSync('git', ['sparse-checkout', '--help']).status;
  return returnCode === 0;
}

/**
 * Check out source code of the lldb for headers
 * @param {string} lldbVersion Version of lldb, either like 3.9 or 39
 * @param {string} buildDir Path to the llnode module directory
 * @returns {string} The include directory in the downloaded lldb source code
 */
function cloneHeaders(lldbVersion, buildDir) {
  const lldbHeadersBranch = versionToBranch(lldbVersion);
  const lldbInstallDir = path.resolve(buildDir, `lldb-${lldbVersion}`);

  if (!fs.existsSync(lldbInstallDir)) {
    console.log(`\nCloning lldb ${lldbHeadersBranch} into ${lldbInstallDir}`);
    if (doesGitSupportSparseCheckout()) {
      // use `git clone --filter` in git v2.19 to only download `lldb` dir of `llvm-project` monorepo
      // see: https://stackoverflow.com/a/52269934/3117331
      // see: https://github.blog/2020-01-17-bring-your-monorepo-down-to-size-with-sparse-checkout/
      child_process.execFileSync(
          'git', ['clone',
            '--depth', '1',
            '--filter=blob:none',
            '--sparse',
            '--branch', lldbHeadersBranch,
            'https://github.com/llvm/llvm-project.git',
            lldbInstallDir
          ],
          { stdio: 'inherit' });  // show progress
      child_process.execFileSync(
          'git', [
            '-C', lldbInstallDir,
            'sparse-checkout',
            'set', 'lldb'
          ],
          { stdio: 'inherit' });  // show progress
    } else {
      // There's no sparse checkout support, so we need to clone the entire
      // repo.
      child_process.execFileSync(
          'git', ['clone',
            '--depth', '1',
            '--branch', lldbHeadersBranch,
            'https://github.com/llvm/llvm-project.git',
            lldbInstallDir
          ],
          { stdio: 'inherit' });  // show progress
    }
  } else {
    console.log(`\nSkip cloning lldb headers because ${lldbInstallDir} exists`);
  }
  return path.join(lldbInstallDir, 'lldb', 'include');
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
 * Find a directory containing a LLVM executable in Windows.
 * The search happens in the following order:
 * - the directory specified by the user using npm --lldb_exe=...
 * - using 'where' to find the executable in the PATH
 * - the default LLVM location in Program Files
 * Returns undefined if the executable was not found.
 * @param {string} exeName
 * @returns {string|undefined}
 */
function findWindowsExeDir(exeName) {
  // Look for exeName at the location of lldb_exe
  if (process.env.npm_config_lldb_exe !== undefined) {
    const exeDir = path.dirname(process.env.npm_config_lldb_exe);
    if (fs.existsSync(path.join(exeDir, exeName))) {
      return exeDir;
    }
    console.log(`Could not find ${exeName} in the directory of lldb_exe`);
  }

  // Look for exeName in the PATH
  let exePath;
  try {
    exePath = child_process.execSync(
        `where ${exeName}`,
        { stdio: 'pipe' }  // to suppress stderr
    ).toString().trim().split(/[\r\n]+/g)[0].trim();
  } catch (err) { /* Do nothing. */ }
  // Ensure the string returned by 'where' is not an error
  if (exePath && fs.existsSync(exePath)) {
    return path.dirname(exePath);
  }
  console.log(`Could not find ${exeName} in the PATH`);

  // Look for exeName in Program Files
  if (process.env['ProgramFiles']) {
    const exeDir = path.join(process.env['ProgramFiles'], 'LLVM', 'bin');
    if (fs.existsSync(path.join(exeDir, exeName))) {
      return exeDir;
    }
  }
  if (process.env['ProgramFiles(x86)']) {
    const exeDir = path.join(process.env['ProgramFiles(x86)'], 'LLVM', 'bin');
    if (fs.existsSync(path.join(exeDir, exeName))) {
      return exeDir;
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
  const versionMatch = lldbStr.match(/version (\d+.\d+)/);
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
  findWindowsExeDir,
  getLldbVersion
};
