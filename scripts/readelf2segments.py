#!/usr/bin/env python
'''
Created on 7 Apr 2016

@author: hhellyer
'''
import sys,os,subprocess

READELF_COMMAND = "readelf --segments {0}"

# Grab the details of the memory ranges in the process from readelf.
def main():
    if( len(sys.argv) < 2 ):
        print("Usage " + sys.argv[0] + " <core_file>")
        sys.exit(1)
    core_file = sys.argv[1]

    readelf_proc = subprocess.Popen(
        READELF_COMMAND.format(core_file), shell=True, bufsize=1,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, close_fds=True)
    (readelf_stdin, readelf_stdout) = (readelf_proc.stdin, readelf_proc.stdout)

    reading_segment = False
    vaddress = ""

    for line in readelf_stdout:
        line = line.strip()
        if not line.startswith("LOAD") and not reading_segment:
            continue
        elif line.startswith("LOAD"):
            # Might need to filer out segments that have a file offset of 0
            # (ie segments that aren't in the core!)
            reading_segment = True
            (type, offset, vaddress, paddr) = line.split()
        elif reading_segment:
            reading_segment = False
            memsize = line.split()[1]
            # Simple format "address size", both in hex
            print("{0} {1}".format(vaddress, memsize))
            vaddress = ""

if __name__ == '__main__':
    main()
