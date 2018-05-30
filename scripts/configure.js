'use strict';

const os = require('os');
const fs = require('fs');
const path = require('path');

const lldb = require('./lldb');
function main() {
  const buildDir = process.cwd();
  console.log('Build dir is: ' + buildDir);
  const osName = os.type();
  const result = configureInstallation(osName, buildDir);
  writeConfig(result.config);
  writeLlnodeScript(buildDir, result.executable, osName);
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
  let includeDir;
  const config = {
    variables: {}
  };

  if (osName === 'Darwin') {
    installation = require('./darwin').getLldbInstallation();
    if (installation.libDir) {
      config.variables['lldb_lib_dir%'] = installation.libDir;
    }
  } else if (osName === 'Linux') {
    installation = require('./linux').getLldbInstallation();
    if (installation.libDir) {
      config.variables['lldb_lib_dir%'] = installation.libDir;
    }
    config.variables['lldb_lib%'] = installation.libName;
  } else if (osName === 'FreeBSD') {
    installation = require('./freebsd').getLldbInstallation();
    config.variables['lldb_lib_dir%'] = installation.libDir;
  } else {
    console.log(`Unsupported OS: ${osName}`);
    process.exit(1);
  }

  if (installation.includeDir === undefined) {
    // Could not find the headers, need to download them
    includeDir = lldb.cloneHeaders(installation.version, buildDir);
  } else {
    includeDir = installation.includeDir;
  }
  config.variables['lldb_include_dir%'] = includeDir;
  return {
    executable: installation.executable,
    config
  };
}

/**
 * Generate the llnode shortcut script
 * @param {string} buildDir Path of the project directory
 * @param {string} lldbExe Name of the lldb executable
 * @param {string} osName Name of the OS
 */
function writeLlnodeScript(buildDir, lldbExe, osName) {
  const text = scriptText(osName, lldbExe);
  const scriptPath = path.join(buildDir, 'llnode.js');
  console.log(`Writing llnode.js shortcut to ${scriptPath}`);
  fs.writeFileSync(scriptPath, text);
}

/**
 * Write configuration to config.gypi
 * @param {string} config
 */
function writeConfig(config) {
  const options = JSON.stringify(config, null, 2);
  fs.writeFileSync('config.gypi', options, 'utf-8');
  console.log('Writing config.gypi:');
  console.log(options);
}

function scriptText(osName, lldbExe) {
  let lib = 'llnode.so';
  if (osName === 'Darwin') { lib = 'llnode.dylib'; }

  return `#!/usr/bin/env node

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');

const scriptPath = path.dirname(__filename);
const globalPath = path.resolve(
  scriptPath, '..', 'lib', 'node_modules', 'llnode', '${lib}');

let llnodePlugin;
if (path.basename(scriptPath) === '.bin') {
  // llnode installed locally in node_modules/.bin
  llnodePlugin = path.resolve(scriptPath, '..', 'llnode', '${lib}');
} else if (fs.existsSync(globalPath)) {
  // llnode installed globally in lib/node_modules
  llnodePlugin = globalPath;
} else {
  // The script is invoked directly
  llnodePlugin = path.join(scriptPath, '${lib}');
}

try {
  child_process.execFileSync(
    '${lldbExe}', [
      '--one-line',
      'plugin load \\''+llnodePlugin+'\\'',
      '--one-line',
      'settings set prompt \\'(llnode) \\''
    ].concat(process.argv.slice(2)), {
      stdio: 'inherit'
    });
} catch (err) {
  process.exit(err.status);
}
`;
}
