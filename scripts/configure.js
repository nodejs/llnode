const os = require('os');
const fs = require('fs');
const child_process = require('child_process');

const lldbReleases = {
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
if (osName === 'Darwin') {

  lldbVersion = getDarwinRelease();

  if(lldbVersion === undefined) {
    console.log('Unable to locate lldb binary. llnode installation failed.');
    process.exit(1);
  }

  lldbHeadersBranch = lldbReleases[lldbVersion];
  lldbIncludeDir = 'lldb-' + lldbVersion;

} else if ( osName === 'Linux') {

  lldbVersion = getLinuxVersion();

  if(lldbVersion === undefined) {
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

console.log('Installing llnode for lldb version ' + lldbVersion);

// Check out source code of the LLDB that is compatible with OS X's default lldb
// TODO: The llvm project is probably moving to github soon at that point we
// should stop using the mirror.
if (lldbHeadersBranch != undefined) {
  console.log('Cloning lldb from ' + lldbHeadersBranch);
  child_process.execFileSync('git', ['clone', '--depth=1', '-b', lldbHeadersBranch, 'https://github.com/llvm-mirror/lldb.git', lldbIncludeDir],
      {cwd: buildDir});
}

// Link to the headers file so we can run gyp_llnode directly and don't need to
// setup
// parameters to pass it.
console.log('Linking lldb to include directory ' + lldbIncludeDir);
fs.symlinkSync(lldbIncludeDir, 'lldb');

// Initialize GYP
// We can use the node-gyp that comes with npm.
// We can locate it with npm -g explore npm npm explore node-gyp pwd
// It might have been neater to make node-gyp one of our dependencies
// *but* they don't get installed until after the install step has run.
var gypDir = child_process.execFileSync('npm', ['-g', 'explore', 'npm', 'npm', 'explore', 'node-gyp', 'pwd'],  {cwd: buildDir}).toString().trim();
fs.mkdirSync('tools');
console.log('Linking tools/gyp to ' + gypDir+'/gyp');
fs.symlinkSync(gypDir + '/gyp', 'tools/gyp');

// Exit with success.
process.exit(0);

// On Mac the lldb version string doesn't match the original lldb versions.
function getDarwinRelease() {
  var xcodeStr;
  try {
    xcodeStr = child_process.execFileSync('xcodebuild', ['-version']).toString();
  } catch(err) {
    return undefined;
  }
  var versionStr = '';
    var splitStr = xcodeStr.split(os.EOL);
    for( var str of splitStr) {
      if (str.indexOf('Xcode') != -1) {
        versionStr = str.split(' ')[1];
        break;
      }
    }
  // console.log('Xcode version is ' + version_str)

  var version = parseFloat(versionStr);
  if (version > 8.0) {
    return '3.8';
  } else {
    return '3.4';
  }
}

// There are multiple versions of lldb available for the various linux distos.
// Use the default unless --llnode_version= has been set on the command line.
function getLinuxVersion() {

  // TODO - Allow the user to specify the version of lldb to use, e.g:
  // lldb-3.9 instead of lldb as Linux has packages for multiple versions
  // of lldb.
  var lldbStr;
  try {
    lldbStr = child_process.execFileSync('lldb', ['-v']).toString();
  } catch(err) {
    return undefined;
  }
  // Ignore minor revisions like 3.8.1
  if (lldbStr.indexOf('version 3.9') > 0) {
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
    var includeDir = child_process.execFileSync('llvm-config-' + version, ['--prefix']).toString().trim();
    // console.log('Checking for directory ' + include_dir);
    // Include directory doesn't need include/lldb on the end but the llvm
    // headers can be installed without the lldb headers so check for them.
    if (fs.existsSync(includeDir + '/include/lldb')) {
      // console.log('Found ' + include_dir);
      return includeDir;
    }
  } catch(err) {
    // Return undefined, we will download the headers.
  }
  // On Redhat the headers are just installed in /usr/include
  if (fs.existsSync('/usr/include/lldb')) {
    return '/usr';
  }
  return undefined;
}
