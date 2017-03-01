#!/usr/bin/env node
'use strict';

const assert = require('assert');
const fs = require('fs');
const Buffer = require('buffer').Buffer;

const ALIGN_64 = 8;

const HEADER_SIZE = 4 * 7;
const BODY_OFF_64 = 4 * 8;
const MAGIC_LE_32 = 0xfeedface;
const MAGIC_LE_64 = 0xfeedfacf;

const CMD_HEADER_SIZE = 8;
const LC_SEGMENT_64 = 0x19;
const LC_SEGMENT_64_SIZE = 4 * 2 + 16 + 8 * 4 + 4 * 4;
const VM_PROT_WRITE = 0x2;

if (process.argv.length < 3) {
  console.error('Usage: %s <core_file>', process.argv[1]);
  process.exit(1);
  return;
}

const fd = fs.openSync(process.argv[2], 'r');

function read(start, end) {
  const data = Buffer.alloc(end - start);
  const bytesRead = fs.readSync(fd, data, 0, data.length, start);

  assert.equal(data.length, bytesRead, 'Read less than expected');

  return data;
}

function parseUInt64LE(buf, off) {
  const low = buf.readUInt32LE(off);
  const high = buf.readUInt32LE(off + 4);

  let r = high.toString(16);
  while (r.length < 8)
    r = '0' + r;
  r += low.toString(16);
  while (r.length < 16)
    r = '0' + r;
  return r;
}

/* header = { magic, cpu, cpu_sub, filetype, ncmds, sizeofcmds, flags } */
const header = read(0, HEADER_SIZE);
assert.equal(header.readUInt32LE(0), MAGIC_LE_64, 'Invalid magic value');

const ncmds = header.readUInt32LE(4 * 4);
const sizeofcmds = header.readUInt32LE(4 * 5);

for (let off = BODY_OFF_64, i = 0; i < ncmds; i++) {
  /* cmd = { type, size } */
  const cmd = read(off, off + CMD_HEADER_SIZE);

  const type = cmd.readUInt32LE(0);
  const size = cmd.readUInt32LE(4);
  off += size;

  if (type !== LC_SEGMENT_64)
    continue;

  assert.equal(size, LC_SEGMENT_64_SIZE, 'Invalid LC_SEGMENT_64 size');

  const body = read(off - size + 4 * 2, off);
  const vmaddr = parseUInt64LE(body, 16);
  const vmsize = parseUInt64LE(body, 24);
  const prot = body.readUInt32LE(48) | body.readUInt32LE(52);
  if (!(prot & VM_PROT_WRITE))
    continue;

  console.log(vmaddr + ' ' + vmsize);
}
