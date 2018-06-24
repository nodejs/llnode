
'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const lldb = require('./lldb');

/**
 * Find the 'best' lldb to use, exit the process with 1 if failed.
 * The search happens in the following order:
 * - the one specified by the user using npm --lldb_exe=... install llnode
 * - the default lldb executable (`lldb`)
 * - the higest known lldb version (`lldb-x.y`)
 * - the names of future releases are predictable for linux
 *
 * @returns {string} Path to the lldb executable
 */
function getLldbExecutable() {
  if (process.env.npm_config_lldb_exe !== undefined) {
    return process.env.npm_config_lldb_exe;
  }

  const lldbExeNames = [
    'lldb', 'lldb-5.0', 'lldb-4.0',
    'lldb-3.9', 'lldb-3.8', 'lldb-3.7', 'lldb-3.6'
  ];

  return lldb.tryExecutables(lldbExeNames);
}

/**
 * Try to find an executable llvm-config in the system. Returns undefined
 * it it could not be found.
 * @param {string} version lldb version for linux, e.g. '3.9' for lldb-3.9
 * @returns {string|undefined}
 */
function getLlvmConfig(version) {
  // On Ubuntu llvm-config is suffixed
  let llvmConfig = lldb.tryExecutables([`llvm-config-${version}`]);
  if (!llvmConfig) {
    llvmConfig = lldb.tryExecutables(['llvm-config']);
  }
  if (llvmConfig) {
    try {
      child_process.execFileSync(llvmConfig, ['--version']);
    } catch (err) {
      return undefined;  // Not really executable
    }
  }

  return llvmConfig;
}

/**
 * Get the directory containing the lldb headers. If it returns undefined,
 * we need to download the headers to ./lldb/include/lldb.
 * Using the installed headers will ensure we have the correct ones.
 * @param {string|undefined} llvmConfig Path to llvm-config, if it's installed
 * @returns {string|undefined} the lldb include directory, undefined if failed
 */
function getIncludeDir(llvmConfig) {
  if (llvmConfig) {
    const includeDir = child_process.execFileSync(
        llvmConfig, ['--includedir']
    ).toString().trim();
    // Include directory doesn't need include/lldb on the end but the llvm
    // headers can be installed without the lldb headers so check for them.
    if (includeDir && fs.existsSync(lldb.getApiHeadersPath(includeDir))) {
      return includeDir;
    }
  }

  // On RedHat the headers are just installed in /usr/include
  if (fs.existsSync(lldb.getApiHeadersPath('/usr/include'))) {
    return '/usr/include';
  }

  // We will download the headers
  return undefined;
}

/**
 * Get the directory containing the lldb shared libraries. If it returns
 * undefined, the shared library will be searched from the global search paths
 * @param {string} lldbExe Path to the corresponding lldb executable
 * @param {string|undefined} llvmConfig Path to llvm-config, if it's installed
 * @returns {{dir:string|undefined, name:string, so: string|undefined}}
 */
function getLib(lldbExe, llvmConfig) {
  // First look for the libraries in the directory returned by
  // llvm-config --libdir
  if (llvmConfig) {
    const libDir = child_process.execFileSync(
        llvmConfig, ['--libdir']
    ).toString().trim();
    if (fs.existsSync(path.join(libDir, 'liblldb.so'))) {
      return { dir: libDir, name: 'lldb', so: undefined };
    }
  }

  // Look for the libraries loaded by the executable using ldd
  let libs;
  try {
    libs = child_process.execFileSync(
        'ldd', [lldbExe]
    ).toString().trim();
  } catch (err) {
    return { dir: undefined, name: 'lldb', so: undefined };
  }

  const lib = libs.split(/\s/).find(
      line => line.includes('liblldb') && line.startsWith('/'));
  if (!lib) {
    return { dir: undefined, name: 'lldb', so: undefined };
  }

  console.log(`From ldd: ${lldbExe} loads ${lib}`);

  const libDir = path.dirname(path.resolve(lib));
  // On Ubuntu the libraries are suffixed and installed globally
  const libName = path.basename(lib).match(/lib(lldb.*?)\.so/)[1];

  // On RedHat there might not be a non-versioned liblldb.so
  // in the system. It's fine in the case of plugins since the lldb executable
  // will load the library before loading the plugin, but we will have to link
  // to the versioned library file for addons.
  if (fs.existsSync(path.join(libDir, `lib${libName}.so`))) {
    return {
      dir: libDir,
      name: libName,
      so: undefined
    };
  }

  return {
    dir: undefined,
    name: libName,
    so: lib
  };
}

/**
 * Get the lldb installation. If prefix is undefined, the headers need to
 * be downloaded.
 * The version string will be in the form like '3.9'
 */
function getLldbInstallation() {
  console.log('Looking for lldb executable...');
  const lldbExe = getLldbExecutable();
  if (!lldbExe) {
    console.log('Could not find any usable lldb executable');
    console.log('Please see the README.md on how to install lldb');
    process.exit(1);
  }
  console.log(`Found lldb executable ${lldbExe}`);

  console.log('\nReading lldb version...');
  const lldbVersion = lldb.getLldbVersion(lldbExe);
  if (!lldbVersion) {
    console.log(`Unable to get the version from the lldb binary ${lldbExe}`);
    process.exit(1);
  }
  console.log(`Installing llnode for ${lldbExe}, lldb version ${lldbVersion}`);

  console.log(`\nLooking for llvm-config for lldb ${lldbVersion}...`);
  const llvmConfig = getLlvmConfig(lldbVersion);
  if (!llvmConfig) {
    console.log('No llvm-config found');
  } else {
    console.log(`Using llvm-config in ${llvmConfig}`);
  }

  console.log(`\nLooking for headers for lldb ${lldbVersion}...`);
  const includeDir = getIncludeDir(llvmConfig);
  if (!includeDir) {
    console.log('Could not find the headers, will download them later');
  } else {
    console.log(`Found lldb headers in ${includeDir}`);
  }

  console.log(`\nLooking for shared libraries for lldb ${lldbVersion}...`);
  const lib = getLib(lldbExe, llvmConfig);
  if (!lib.dir) {
    console.log(`Could not find non-versioned lib${lib.name}.so in the system`);
    console.log('Plugin symbols will be resolved by the lldb executable ' +
                'at runtime');
    console.log(`Addon will be linked to ${lib.so}`);
  } else {
    console.log(`Found lib${lib.name}.so in ${lib.dir}`);
  }

  return {
    executable: lldbExe,
    version: lldbVersion,
    includeDir: includeDir,
    libDir: lib.dir,
    libName: lib.name,
    libPath: lib.so
  };
}

module.exports = {
  getLldbInstallation
};
