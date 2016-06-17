#!/usr/bin/env python
'''
Created on 7 Apr 2016

@author: hhellyer
'''
import sys,os,subprocess

OTOOL_COMMAND = "otool -l {0}"

# Grab the details of the memory ranges in the process from otool.
def main():
    if( len(sys.argv) < 2 ):
        print("Usage " + sys.argv[0] + " <core_file>")
        sys.exit(1)
    core_file = sys.argv[1]

    otool_proc = subprocess.Popen(
        OTOOL_COMMAND.format(core_file), shell=True, bufsize=1,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, close_fds=True)
    (otool_stdin, otool_stdout) = (otool_proc.stdin, otool_proc.stdout)

    reading_segment = False
    vaddress = ""

    for line in otool_stdout:
        line = line.strip()
        if not line.startswith("vmaddr") and not reading_segment:
            continue
        elif line.startswith("vmaddr"):
            # Might need to filer out segments that have a file offset of 0
            # (ie segments that aren't in the core!)
            reading_segment = True
            (name, vaddress) = line.split()
        elif line.startswith("vmsize") and reading_segment:
            reading_segment = False
            memsize = line.split()[1]
            # Simple format "address size", both in hex
            print("{0} {1}".format(vaddress, memsize))
            vaddress = ""

if __name__ == '__main__':
    main()
