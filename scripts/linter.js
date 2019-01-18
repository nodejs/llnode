'use strict';

const { getNativeBinary } = require("clang-format");
const { exec } = require("child_process");
const { readdir, readFile } = require("fs");

const dirtyFiles = [];

process.on("exit", () => {
  if (dirtyFiles.length == 0) {
    return;
  }
  process.exitCode = 1;
  process._rawDebug("The following files are not formatted correctly:");
  for (let file of dirtyFiles) {
    process._rawDebug(`  ${file}`);
  }

});

readdir("./src/", (err, files) => {
  for (let file of files) {
    readFile(`./src/${file}`, {encoding: 'utf8'}, (err, data) => {
      if (err) {
        console.error(err);
        process.exitCode = 2;
        return;
      }
      exec(`${getNativeBinary()} src/${file}`, {maxBuffer: 1024 * 1024 * 5}, (err, stdout, stderr) => {
        if (err) {
          console.error(err);
          process.exitCode = 2;
          return;
        }
        if (stdout.trim() != data.trim()) {
          dirtyFiles.push(file);
        }
      });
    });
  }
});
