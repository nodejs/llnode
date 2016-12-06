var os = require('os');
var child_process = require('child_process');

var cwd = process.cwd();
var osName = os.type();
var libExt = "so";

if (osName === 'Darwin') {
  libExt = "dylib";
}

var llnodeLib = "llnode." + libExt;

// Move the library somewhere easy to remember.
console.log('Copying ' + cwd + '/out/Release/' + llnodeLib + ' to ' + cwd + '/' + llnodeLib);
child_process.execSync('mv ' + cwd + '/out/Release/' + llnodeLib + ' ' + cwd + '/' + llnodeLib );

console.log(os.EOL + 'llnode plugin installed, load in lldb with:');
console.log('(lldb) plugin load ' + cwd + '/' + llnodeLib + os.EOL);
