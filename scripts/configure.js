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
    if (installation.libPath) {
      config.variables['lldb_lib_so%'] = installation.libPath;
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
  const scriptPath = path.join(buildDir, 'llnode.sh');
  console.log(`Writing llnode.sh shortcut to ${scriptPath}`);
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
