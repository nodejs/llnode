'use strict';

const os = require('os');
const fs = require('fs');
const child_process = require('child_process');

const lldbReleases = {
  '4.0': 'release_40',
  '3.9': 'release_39',
  '3.8': 'release_38',
  '3.7': 'release_37',
  '3.6': 'release_36',
  '3.5': 'release_35',
  '3.4': 'release_34',
};

const buildDir = process.cwd();

console.log('Build dir is: ' + buildDir);

const osName = os.type();

var lldbVersion;
var lldbHeadersBranch;
var lldbIncludeDir;

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

  lldbHeadersBranch = lldbReleases[lldbVersion];
  lldbIncludeDir = 'lldb-' + lldbVersion;

} else if (osName === 'Linux') {

  lldbExe = getLldbExecutable();
  lldbVersion = getLinuxVersion(lldbExe);

  if (lldbVersion === undefined) {
    console.log('Unable to locate lldb binary. llnode installation failed.');
    process.exit(1);
  }

  // console.log('lldb_version is ' + lldb_version)
  var installedHeadersDir = getLinuxHeadersDir(lldbVersion);
  // console.log('installed_headers_dir is ' + installed_headers_dir);
  if (installedHeadersDir === undefined) {
    // Initialising lldb_headers_branch will cause us to clone them.
    lldbHeadersBranch = lldbReleases[lldbVersion];
    lldbIncludeDir = 'lldb-' + lldbVersion;
  } else {
    lldbIncludeDir = installedHeadersDir;
  }
}

console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);

// Check out source code of the LLDB that is compatible with OS X's default lldb
// TODO: The llvm project is probably moving to github soon at that point we
// should stop using the mirror.
if (lldbHeadersBranch != undefined) {
  console.log('Cloning lldb from ' + lldbHeadersBranch);
  child_process.execSync(`rm -rf ${lldbIncludeDir}`);
  child_process.execFileSync('git',
    ['clone', '--depth=1', '-b', lldbHeadersBranch,
      'https://github.com/llvm-mirror/lldb.git', lldbIncludeDir],
    {cwd: buildDir});
}

// Link to the headers file so we can run gyp_llnode directly and don't need to
// setup parameters to pass it.
console.log(`Linking lldb to include directory ${lldbIncludeDir}`);
try {
  fs.unlinkSync('lldb');
} catch (error) {
  // File does not exist, no need to handle.
}
fs.symlinkSync(lldbIncludeDir, 'lldb');

// npm explore has a different root folder when using -g
// So we are tacking on the extra the additional subfolders
var gypSubDir = 'node-gyp';
if (process.env.npm_config_global) {
  gypSubDir = 'npm/node_modules/node-gyp';
}

// Initialize GYP
// We can use the node-gyp that comes with npm.
// We can locate it with npm -g explore npm npm explore node-gyp pwd
// It might have been neater to make node-gyp one of our dependencies
// *but* they don't get installed until after the install step has run.
var gypDir = child_process.execFileSync('npm',
  ['-g', 'explore', 'npm', 'npm', 'explore', gypSubDir, 'pwd'],
  {cwd: buildDir}).toString().trim();
child_process.execSync('rm -rf tools');
fs.mkdirSync('tools');
console.log(`Linking tools/gyp to ${gypDir}/gyp`);
fs.symlinkSync(`${gypDir}/gyp`, 'tools/gyp');

fs.writeFileSync(`${buildDir}/scripts/llnode.sh`, scriptText(lldbExe));

// Exit with success.
process.exit(0);

// On Mac the lldb version string doesn't match the original lldb versions.
function getDarwinRelease() {
  var xcodeStr;
  try {
    xcodeStr = child_process.execFileSync('xcodebuild', ['-version'])
    .toString();
  } catch (err) {
    return undefined;
  }
  var versionStr = '';
  var splitStr = xcodeStr.split(os.EOL);
  for (var str of splitStr) {
    if (str.indexOf('Xcode') != -1) {
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

// Find the 'best' lldb to use. Either:
// - the one specified by the user using npm --lldb_exe=... install llnode
// - the default lldb executable
// - the higest known lldb version
function getLldbExecutable() {
  var lldbExe = process.env.npm_config_lldb_exe;
  if (lldbExe === undefined) {
    var lldbExeNames = ['lldb', 'lldb-4.0', 'lldb-3.9',
      'lldb-3.8', 'lldb-3.7', 'lldb-3.6'];
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
  if (lldbStr.indexOf('version 4.0') > 0) {
    return '4.0';
  } else if (lldbStr.indexOf('version 3.9') > 0) {
    return '3.9';
  } else if (lldbStr.indexOf('version 3.8') > 0) {
    return '3.8';
  } else if (lldbStr.indexOf('version 3.7') > 0) {
    return '3.7';
  } if (lldbStr.indexOf('version 3.6') > 0) {
    return '3.6';
  } if (lldbStr.indexOf('version 3.5') > 0) {
    return '3.5';
  }
  return undefined;
}

function getLinuxHeadersDir(version) {
  // Get the directory which should contain the headers and
  // check if they are present.
  // (Using the installed headers will ensure we have the correct ones.)
  console.log('Checking for headers, version is ' + version);
  try {
    var includeDir = child_process.execFileSync('llvm-config-' + version,
      ['--prefix']).toString().trim();
    // console.log('Checking for directory ' + include_dir);
    // Include directory doesn't need include/lldb on the end but the llvm
    // headers can be installed without the lldb headers so check for them.
    if (fs.existsSync(includeDir + '/include/lldb')) {
      // console.log('Found ' + include_dir);
      return includeDir;
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

LLNODE_SCRIPT=\`node -p "path.resolve('$0')"\`

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
