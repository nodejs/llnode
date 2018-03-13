'use strict';

const os = require('os');
const fs = require('fs');
const path = require('path');
const child_process = require('child_process');

const buildDir = process.cwd();

console.log('Build dir is: ' + buildDir);

const osName = os.type();

var lldbVersion; // Similar to what `llvm-config --version` returns
var lldbInstallDir;  // Where the lldb installation is, `llvm-config --prefix`

// Need to determine:
// - What level of lldb we are running.
// - If we need the headers. (Linux may have them installed)
var lldbExe = 'lldb';

if (osName === 'Darwin') {
  lldbVersion = getDarwinRelease();

  if (lldbVersion === undefined) {
    console.log('Unable to locate lldb binary. llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getDarwinInstallDir();
  if (installedDir === undefined) {
    const lldbHeadersBranch = lldbVersionToBranch(lldbVersion);
    lldbInstallDir = 'lldb-' + lldbVersion;
    cloneHeaders(lldbHeadersBranch, lldbInstallDir, buildDir);
    fs.writeFileSync('options.gypi', '{}', 'utf-8');
  } else {
    lldbInstallDir = installedDir;
    setDarwinBuildDir();
  }
} else if (osName === 'Linux') {
  lldbExe = getLldbExecutable();
  lldbVersion = getLinuxVersion(lldbExe);

  if (lldbVersion === undefined) {
    console.log('Unable to locate lldb binary. llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getLinuxInstallDir(lldbVersion);
  if (installedDir === undefined) {
    const lldbHeadersBranch = lldbVersionToBranch(lldbVersion);
    lldbInstallDir = 'lldb-' + lldbVersion;
    cloneHeaders(lldbHeadersBranch, lldbInstallDir, buildDir);
    fs.writeFileSync('options.gypi', '{}', 'utf-8');
  } else {
    lldbInstallDir = installedDir;
  }
} else if (osName === 'FreeBSD') {
  lldbExe = getLldbExecutable();
  lldbVersion = getFreeBSDVersion(lldbExe);

  if (lldbVersion === undefined) {
    console.log('Unable to locate lldb binary. llnode installation failed.');
    process.exit(1);
  }

  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);
  const installedDir = getFreeBSDInstallDir(lldbVersion);
  if (installedDir === undefined) {
    // As this is a BSD we know this system is in an improper state
    // So we can exit with an error
    console.log('The system isn\'t set up correcly.');
    console.log('Try `pkg install llvm39');
    console.log('And `ln -s /usr/local/bin/lldb39 /usr/bin/lldb`');
    process.exit(1);
  } else {
    lldbInstallDir = installedDir;
  }
}

// This should be equivalent to `llvm-config --includedir`/lldb
function getLldbHeadersPath(lldbInstallDir) {
  return path.join(lldbInstallDir, 'include', 'lldb');
}

// Check out source code of the LLDB for headers
// TODO: The llvm project is probably moving to github soon at that point we
// should stop using the mirror.
function cloneHeaders(lldbHeadersBranch, lldbInstallDir, buildDir) {
  const lldbHeaders = getLldbHeadersPath(lldbInstallDir);
  if (!fs.existsSync(lldbInstallDir)) {
    console.log(`Cloning lldb from ${lldbHeadersBranch} to ${lldbInstallDir}`);
    child_process.execFileSync('git',
      ['clone', '--depth=1', '-b', lldbHeadersBranch,
        'https://github.com/llvm-mirror/lldb.git', lldbInstallDir],
      {
        cwd: buildDir,
        stdio: 'inherit'  // show progress
      });
  } else {
    console.log(`Skip cloning lldb headers because ${lldbHeaders} exists`);
  }
}

// Link to the headers file so we can run gyp_llnode directly and don't need to
// setup parameters to pass it.
console.log(`Linking lldb to installation directory ${lldbInstallDir}`);
try {
  fs.unlinkSync('lldb');
} catch (error) {
  // File does not exist, no need to handle.
}
fs.symlinkSync(lldbInstallDir, 'lldb');

// npm explore has a different root folder when using -g
// So we are tacking on the extra the additional subfolders
var gypSubDir = 'node-gyp';
if (process.env.npm_config_global) {
  gypSubDir = 'npm/node_modules/node-gyp';
}

// npm can be in a different location than the current
// location for global installs so we need find out where the npm is
var npmLocation = child_process.execFileSync('which', ['npm']);
var npmModules = path.join(npmLocation.toString(), '../../lib/node_modules/npm');

// Initialize GYP
// We can use the node-gyp that comes with npm.
// We can locate it with npm -g explore npm npm explore node-gyp pwd
// It might have been neater to make node-gyp one of our dependencies
// *but* they don't get installed until after the install step has run.
var gypDir = child_process.execFileSync('npm',
  ['-g', 'explore', npmModules, 'npm', 'explore', gypSubDir, 'pwd'],
  {cwd: buildDir}).toString().trim();
child_process.execSync('rm -rf tools');
fs.mkdirSync('tools');
console.log(`Linking tools/gyp to ${gypDir}/gyp`);
fs.symlinkSync(`${gypDir}/gyp`, 'tools/gyp');

fs.writeFileSync(`${buildDir}/llnode.sh`, scriptText(lldbExe));

// Exit with success.
process.exit(0);

function lldbVersionToBranch(version) {
  return 'release_' + version.replace('.', '');
}

// On Mac the lldb version string doesn't match the original lldb versions.
function getDarwinRelease() {
  var versionFromConfig;
  try {
    versionFromConfig = child_process.execFileSync('llvm-config', [
      '--version'
    ]).toString().trim();
  } catch (err) {
    // No llvm-config, try to get the version from xcodebuild
  }
  if (versionFromConfig !== undefined) {
    return versionFromConfig.split('.').slice(0, 2).join('.');
  }

  var xcodeStr;
  try {
    xcodeStr = child_process.execFileSync('xcodebuild', [
      '-version'
    ]).toString();
  } catch (err) {
    return undefined;
  }
  var versionStr = '';
  var splitStr = xcodeStr.split(os.EOL);
  for (var str of splitStr) {
    if (str.includes('Xcode')) {
      versionStr = str.split(' ')[1];
      break;
    }
  }
  // console.log('Xcode version is ' + version_str)

  var version = parseFloat(versionStr);
  if (version >= 8.3) {
    return '3.9';
  } else if (version > 8.0) {
    return '3.8';
  } else {
    return '3.4';
  }
}

function setDarwinBuildDir() {
  const prefix = child_process.execFileSync('llvm-config', [
    '--prefix'
  ]).toString().trim();
  const options = JSON.stringify({
    variables: { 'lldb_build_dir%': prefix }
  }, null, 2);
  fs.writeFileSync('options.gypi', options, 'utf-8');
  console.log('Overwriting options.gypi with output from llvm-config:');
  console.log(options);
}

function getDarwinInstallDir() {
  var installedDir;
  try {
    installedDir = child_process.execFileSync('llvm-config', [
      '--prefix'
    ]).toString().trim();
  } catch (err) {
    // Return undefined, we will download the headers.
  }
  if (installedDir !== undefined &&
      fs.existsSync(getLldbHeadersPath(installedDir))) {
    return installedDir;
  }
  return undefined;
}

// Find the 'best' lldb to use. Either:
// - the one specified by the user using npm --lldb_exe=... install llnode
// - the default lldb executable
// - the higest known lldb version
// - the names of future releases are predictable for linux
function getLldbExecutable() {
  var lldbExe = process.env.npm_config_lldb_exe;
  if (lldbExe === undefined) {
    var lldbExeNames = [
      'lldb', 'lldb-5.0', 'lldb-4.0',
      'lldb-3.9', 'lldb-3.8', 'lldb-3.7', 'lldb-3.6'
    ];
    for (var lldbExeVersion of lldbExeNames) {
      try {
        lldbExe = child_process.execSync('which ' +
          lldbExeVersion).toString().trim();
        // If the result starts with '/' `which` found a path.
        if (lldbExe.startsWith('/')) {
          break;
        }
      } catch (err) {
        // Do nothing - we expect not to find some of these.
      }
    }
  }
  return lldbExe;
}

// There are multiple versions of lldb available for the various linux distos.
// Use the default unless --llnode_exe= has been set on the command line.
function getLinuxVersion(lldbExe) {
  var lldbStr;
  try {
    lldbStr = child_process.execFileSync(lldbExe, ['-v']).toString();
  } catch (err) {
    return undefined;
  }
  // Ignore minor revisions like 3.8.1
  let versionMatch = lldbStr.match(/version (\d.\d)/);
  if (versionMatch) {
    return versionMatch[1];
  }
  return undefined;
}

// Shim this for consistancy in OS naming
function getFreeBSDVersion(lldbExe) {
  // Strip the dots for BSD
  return getLinuxVersion(lldbExe).replace('.', '');
}

function getFreeBSDInstallDir(version) {
  console.log('Checking for headers, version is ' + version);

  try {
    var installDir = child_process.execFileSync('llvm-config' + version,
      ['--prefix']).toString().trim();
    if (fs.existsSync(installDir + '/include/lldb')) {
      return installDir;
    }
  } catch (err) {
    console.log(installDir + '/include/lldb doesn\'nt exist');
    console.log('Please see README.md');
    console.log(err);
    process.exit(1);
  }
  return undefined;
}

function getLinuxInstallDir(version) {
  // Get the directory which should contain the headers and
  // check if they are present.
  // (Using the installed headers will ensure we have the correct ones.)
  console.log('Checking for headers, version is ' + version);
  try {
    var installDir = child_process.execFileSync('llvm-config-' + version,
      ['--prefix']).toString().trim();
    // console.log('Checking for directory ' + include_dir);
    // Include directory doesn't need include/lldb on the end but the llvm
    // headers can be installed without the lldb headers so check for them.
    if (fs.existsSync(installDir + '/include/lldb')) {
      // console.log('Found ' + include_dir);
      return installDir;
    }
  } catch (err) {
    // Return undefined, we will download the headers.
  }
  // On Redhat the headers are just installed in /usr/include
  if (fs.existsSync('/usr/include/lldb')) {
    return '/usr';
  }
  return undefined;
}

function scriptText(lldbExe) {
  let lib = 'llnode.so';
  if (osName === 'Darwin') {
    lib = 'llnode.dylib';
  }

  return `#!/bin/sh

LLNODE_SCRIPT=\`node -p "require('path').resolve('$0')"\`

SCRIPT_PATH=\`dirname $LLNODE_SCRIPT\`
if [ \`basename $SCRIPT_PATH\` = ".bin" ]; then
  # llnode installed locally in node_modules/.bin
  LLNODE_PLUGIN="$SCRIPT_PATH/../llnode/${lib}"
else
  # llnode installed globally in lib/node_modules
  LLNODE_PLUGIN="$SCRIPT_PATH/../lib/node_modules/llnode/${lib}"
fi

${lldbExe} --one-line "plugin load $LLNODE_PLUGIN" --one-line "settings set prompt '(llnode) '" $@
`;
}
