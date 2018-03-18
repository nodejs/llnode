'use strict';

const os = require('os');
const fs = require('fs');
const path = require('path');
const child_process = require('child_process');

const lldb = require('./lldb');

function main() {
  const buildDir = process.cwd();
  console.log('Build dir is: ' + buildDir);
  const osName = os.type();
  const installation = configureInstallation(osName, buildDir);
  linkHeadersDir(installation.prefix);
  const nodeGypDir = getNodeGyp(buildDir);
  if (nodeGypDir !== undefined) {  // otherwise use tools/gyp
    linkNodeGyp(nodeGypDir);
  }
  writeLlnodeScript(buildDir, installation.executable, osName);
  // Exit with success.
  process.exit(0);
}

main();

/**
 * Get and configure the lldb installation. The returned prefix
 * should be linked to ./lldb
 * @param {string} osName Name of the OS
 * @param {string} buildDir Path of the project directory
 * @returns {{executable: string, version: string, prefix: string}}
 */
function configureInstallation(osName, buildDir) {
  // Need to determine:
  // - What level of lldb we are running.
  // - If we need to download the headers. (Linux may have them installed)
  let installation;
  let prefix;  // Similar to what `llvm-config --prefix` returns

  if (osName === 'Darwin') {
    const darwin = require('./darwin');
    installation = darwin.getLldbInstallation();
    prefix = installation.prefix;
    if (prefix === undefined) {  // Using Xcode installation
      prefix = lldb.cloneHeaders(installation.version, buildDir);
    } else {  // Using custom installation
      // Need to configure with the custom prefix
      const config = {
        variables: { 'lldb_build_dir%': prefix }
      };
      writeConfig(config);
    }
  } else if (osName === 'Linux') {
    const linux = require('./linux');
    installation = linux.getLldbInstallation();
    if (installation.prefix === undefined)
      // Could not find the headers, need to download them
      prefix = lldb.cloneHeaders(installation.version, buildDir);
    else
      prefix = installation.prefix;

    // ./lldb will always be linked to the prefix on Linux
    writeConfig({});
  } else if (osName === 'FreeBSD') {
    const freebsd = require('./freebsd');
    installation = freebsd.getLldbInstallation();
    prefix = installation.prefix;
    // ./lldb will always be linked to the prefix
    writeConfig({});
  } else {
    console.log(`Unsupported OS: ${osName}`);
    process.exit(1);
  }

  return {
    executable: installation.executable,
    version: installation.version,
    prefix
  };
}

/**
 * Link to the headers file so we can run gyp_llnode directly and don't need to
 * setup parameters to pass.
 * @param {string} lldbInstallDir The destination of the symlink
 */
function linkHeadersDir(lldbInstallDir) {
  console.log(`Linking lldb to installation directory ${lldbInstallDir}`);
  try {
    fs.unlinkSync('lldb');
  } catch (error) {
    // File does not exist, no need to handle.
  }
  fs.symlinkSync(lldbInstallDir, 'lldb');
}

/**
 * Get the path to node-gyp installation. Return undefined if tools/gyp
 * exists (possibly already checked out from the gyp source).
 * @param {string|undefined} buildDir  Path of the project directory
 */
function getNodeGyp(buildDir) {
  if (fs.existsSync(path.join(buildDir, 'tools', 'gyp'))) {
    console.log('Found tools/gyp, skip creating the symlink.')
    return undefined;
  }
  // npm explore has a different root folder when using -g
  // So we are tacking on the extra the additional subfolders
  let gypSubDir = 'node-gyp';
  if (process.env.npm_config_global)
    gypSubDir = 'npm/node_modules/node-gyp';

  // npm can be in a different location than the current
  // location for global installs so we need find out where the npm is
  let npmLocation = child_process.execFileSync('which', ['npm']);
  let npmModules = path.join(
      npmLocation.toString(), '../../lib/node_modules/npm');

  // Initialize GYP
  // We can use the node-gyp that comes with npm.
  // We can locate it with npm -g explore npm npm explore node-gyp pwd
  // It might have been neater to make node-gyp one of our dependencies
  // *but* they don't get installed until after the install step has run.
  let gypDir = child_process.execFileSync(
      'npm',
      ['-g', 'explore', npmModules, 'npm', 'explore', gypSubDir, 'pwd'],
      { cwd: buildDir }
  ).toString().trim();
  return gypDir;
}

/**
 * Link tools/gyp to the GYP installation
 * @param {string} nodeGypDir path to node-gyp
 */
function linkNodeGyp(nodeGypDir) {
  if (nodeGypDir !== undefined) {
    child_process.execSync('rm -rf tools');
    fs.mkdirSync('tools');
    console.log(`Linking tools/gyp to ${nodeGypDir}/gyp`);
    fs.symlinkSync(`${nodeGypDir}/gyp`, 'tools/gyp');
  }
}

/**
 * Generate the llnode shortcut script
 * @param {string} buildDir Path of the project directory
 * @param {string} lldbExe Name of the lldb executable
 * @param {string} osName Name of the OS
 */
function writeLlnodeScript(buildDir, lldbExe, osName) {
  const text = scriptText(osName, lldbExe);
  const scriptPath = path.join(buildDir, 'llnode.sh');
  console.log(`Writing llnode.sh shortcut to ${scriptPath}`);
  fs.writeFileSync(scriptPath, text);
}

/**
 * Write configuration to options.gypi
 * @param {string} config
 */
function writeConfig(config) {
  const options = JSON.stringify(config, null, 2);
  fs.writeFileSync('options.gypi', options, 'utf-8');
  console.log('Writing options.gypi:');
  console.log(options);
}

function scriptText(osName, lldbExe) {
  let lib = 'llnode.so';
  if (osName === 'Darwin')
    lib = 'llnode.dylib';

  return `#!/bin/sh

LLNODE_SCRIPT=\`node -p "require('path').resolve('$0')"\`

SCRIPT_PATH=\`dirname $LLNODE_SCRIPT\`
if [ \`basename $SCRIPT_PATH\` = ".bin" ]; then
  # llnode installed locally in node_modules/.bin
  LLNODE_PLUGIN="$SCRIPT_PATH/../llnode/${lib}"
elif [ -e $SCRIPT_PATH/../lib/node_modules/llnode/${lib} ]; then
  # llnode installed globally in lib/node_modules
  LLNODE_PLUGIN="$SCRIPT_PATH/../lib/node_modules/llnode/${lib}"
else
  # The scrips is invoked directly
  LLNODE_PLUGIN="$SCRIPT_PATH/${lib}"
fi

${lldbExe} --one-line "plugin load $LLNODE_PLUGIN" --one-line "settings set prompt '(llnode) '" $@
`;
}
