
'use strict';

const child_process = require('child_process');
const os = require('os');
const fs = require('fs');
const lldb = require('./lldb');

/**
 * On Mac the lldb version string doesn't match the original lldb versions,
 * we need to get it either from `llvm-config --version` (custom installation)
 * or `xcodebuild -version` (Xcode installation).
 *
 * @returns {string|undefined} Deduced version of lldb, undefined if failed
 */
function getLldbVersion() {
  let versionFromConfig;
  try {
    versionFromConfig = child_process.execFileSync('llvm-config', [
      '--version'
    ]).toString().trim();
  } catch (err) {
    // No llvm-config, try to get the version from xcodebuild
  }
  if (versionFromConfig !== undefined) {
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
  let splitStr = xcodeStr.split(os.EOL);
  for (let str of splitStr)
    if (str.includes('Xcode')) {
      xcodeVersion = str.split(' ')[1];
      break;
    }

  if (xcodeVersion === undefined) {
    console.log(`Could not get Xcode version from:\n${xcodeStr}`);
    return undefined;
  }

  let result;
  let version = parseFloat(xcodeVersion);
  if (version >= 8.3)
    result = '3.9';
  else if (version > 8.0)
    result = '3.8';
  else
    result = '3.4';

  if (result !== undefined)
    console.log('Deduced lldb version from Xcode version: ' +
                `Xcode ${xcodeVersion} -> lldb ${result}`);
  else
    console.log('Could not deduce lldb version from Xcode version' +
                xcodeVersion);

  return result;
}

/**
 * Get the directory to the lldb installation, if it returns undefined,
 * we need to download the headers to ./lldb/include/lldb
 * @returns {string|undefined} lldb installation prefix, undefined if failed
 */
function getInstallDir() {
  var installedDir;
  try {
    installedDir = child_process.execFileSync('llvm-config', [
      '--prefix'
    ]).toString().trim();
  } catch (err) {
    // Return undefined, we will download the headers.
  }
  if (installedDir !== undefined &&
      fs.existsSync(lldb.getHeadersPath(installedDir)))
    return installedDir;

  return undefined;
}

/**
 * Get the lldb installation. If prefix is undefined, the headers need to
 * be downloaded.
 * The version string will be in the form like '3.9'
 * @returns {{executable: string, version: string, ?prefix: string}}
 */
function getLldbInstallation() {
  const lldbExe = process.env.npm_config_lldb_exe || 'lldb';
  // We cannot just use the executable specified with
  // process.env.npm_config_lldb_exe to determine the version of lldb
  // because we do not know how. We can only use llvm-config or xcodebuild
  // to retrieve the version.
  const lldbVersion = getLldbVersion();

  if (lldbVersion === undefined) {
    console.log('Unable to deduce the version of lldb, ' +
                'llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getInstallDir();
  return {
    executable: lldbExe,
    version: lldbVersion,
    prefix: installedDir
  };
}

module.exports = {
  getLldbInstallation
};
