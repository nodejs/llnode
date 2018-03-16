'use strict';
const http = require('http')
var fs = require('fs');

// Creates a Timer to be inspected with getactivehandles
setInterval(()=>{}, 500);

// Creates a TCP to be inspected with getactivehandles
const server = http.createServer((req, res) => { res.end('test'); });
server.listen(4321, (err) => {});

// Creates a FSReqWrap to be inspected with getactivehandles
fs.readFile('invalidFile', (err, data) => {});

uncaughtException();
