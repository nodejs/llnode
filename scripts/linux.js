
'use strict';

const child_process = require('child_process');
const fs = require('fs');
const lldb = require('./lldb');

/**
 * Find the 'best' lldb to use, exit the process with 1 if failed.
 * The search happens in the following order:
 * - the one specified by the user using npm --lldb_exe=... install llnode
 * - the default lldb executable (`lldb`)
 * - the higest known lldb version (`lldb-x.y`)
 * - the names of future releases are predictable for linux
 *
 * @returns {string} Name of the lldb executable
 */
function getLldbExecutable() {
  var lldbExe = process.env.npm_config_lldb_exe;
  if (lldbExe !== undefined)
    return lldbExe;

  var lldbExeNames = [
    'lldb', 'lldb-5.0', 'lldb-4.0',
    'lldb-3.9', 'lldb-3.8', 'lldb-3.7', 'lldb-3.6'
  ];

  for (var lldbExeVersion of lldbExeNames)
    try {
      lldbExe = child_process.execSync('which ' +
        lldbExeVersion).toString().trim();
      // If the result starts with '/' `which` found a path.
      if (lldbExe.startsWith('/'))
        break;
    } catch (err) {
      // Do nothing - we expect not to find some of these.
    }

  if (!lldbExe) {
    console.log('Could not find any usable lldb binary');
    console.log('Please see the README.md on how to install lldb');
    process.exit(1);
  }
  return lldbExe;
}

/**
 * Get the lldb version from the lldb executable, exit the process with 1
 * if failed.
 * @param {string} lldbExe
 * @returns {string} Version of the executable in the form like '3.9'
 */
function getLldbVersion(lldbExe) {
  var lldbStr;
  try {
    lldbStr = child_process.execFileSync(lldbExe, ['-v']).toString();
  } catch (err) {
    console.log(err);
    console.log(`Unable to get the version from the lldb binary ${lldbExe}`);
    process.exit(1);
  }
  // Ignore minor revisions like 3.8.1
  const versionMatch = lldbStr.match(/version (\d.\d)/);
  if (versionMatch)
    return versionMatch[1];

  console.log(`Unable to get the version from the lldb binary ${lldbExe}`);
  console.log(`Output from \`${lldbExe} -v\` was ${lldbStr}`);
  process.exit(1);
}

/**
 * Get the directory to the lldb installation, if it returns undefined,
 * we need to download the headers to ./lldb/include/lldb
 * @param {string} version Version of the lldb executable
 * @returns {string|undefined} lldb installation prefix, undefined if failed
 */
function getInstallDir(version) {
  // Get the directory which should contain the headers and
  // check if they are present.
  // (Using the installed headers will ensure we have the correct ones.)
  console.log('Checking for headers, version is ' + version);

  let installDir;
  try {
    installDir = child_process.execFileSync(
        `llvm-config-${version}`,
        ['--prefix']
    ).toString().trim();
  } catch (err) {
    // Could not get the headers through llvm-config, try another way
  }

  if (!installDir)
    // On Redhat the headers are just installed in /usr/include
    if (fs.existsSync('/usr/include/lldb')) {
      return '/usr';
    } else {
      // We will download the headers
      return undefined;
    }

  // Include directory doesn't need include/lldb on the end but the llvm
  // headers can be installed without the lldb headers so check for them.
  const headers = lldb.getHeadersPath(installDir);
  if (fs.existsSync(headers))
    return installDir;

  // We will download the headers
  return undefined;
}

/**
 * Get the lldb installation. If prefix is undefined, the headers need to
 * be downloaded.
 * The version string will be in the form like '3.9'
 * @returns {{executable: string, version: string, ?prefix: string}}
 */
function getLldbInstallation() {
  const lldbExe = getLldbExecutable();
  const lldbVersion = getLldbVersion(lldbExe);

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getInstallDir(lldbVersion);
  return {
    executable: lldbExe,
    version: lldbVersion,
    prefix: installedDir
  };
}

module.exports = {
  getLldbExecutable,
  getLldbVersion,
  getLldbInstallation
};
