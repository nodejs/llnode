""" Helper commands to make development of llnode easier. No API stability
guarantees for these commands (at least for now).

To use it, run:

  (llnode) command script import ./tools/helpers.py

Or start llnode with:

  $ llnode -o 'command script import ./tools/helpers.py'
"""
import struct

import lldb


def int_from_bytearray(uint, size, target):
    byte_order = target.GetByteOrder()

    fmt = ""

    # https://docs.python.org/3.2/library/struct.html#byte-order-size-and-alignment
    # < is little endian, > is big endian
    if byte_order == lldb.eByteOrderLittle:
        fmt += "<"
    elif byte_order == lldb.eByteOrderBig:
        fmt += ">"
    else:
        raise ValueError("Unexpected byte order %d" % byte_order)

    # https://docs.python.org/3.2/library/struct.html#format-characters
    if size == 1:
        fmt += "B"
    elif size == 2:
        fmt += "H"
    elif size == 4:
        fmt += "I"
    elif size == 8:
        fmt += "Q"
    else:
        raise ValueError("Unexpected size %d" % size)
    return struct.unpack(fmt, uint)[0]


def brute(debugger, command, result, internal_dict):
    """ v8-brute <addr>
    This command will try to run `v8 inspect` on 20 object-aligned addresses
    following <addr>. Useful to find all objects on the stack, or all objects
    after a given object field, etc.
    """
    process = debugger.GetSelectedTarget().GetProcess()
    if not command:
        return

    for i in range(20):
        addr = int(command, 16) + (8 * i)
        error = lldb.SBError()
        obj = process.ReadUnsignedFromMemory(addr, 8, error)
        if error.Fail():
            print("failed for 0x%x" % addr)
            continue
        print("v8 inspect 0x%lx" % obj)
        debugger.HandleCommand("v8 inspect 0x%lx" % obj)


def get_constants(debugger, command, result, internal_dict):
    """ v8-get-constants [pattern]
    This command will return all constants prefixed by v8dbg and nodedbg, with
    respective values for each constant. Useful when looking at missing types,
    offsets, etc. Optional pattern parameter will filter symbol names before
    showing them (for example, running `v8-get-constants v8dbg_type_` will
    return all type constants).
    """
    target = debugger.GetSelectedTarget()

    for module in target.module_iter():
        for symbol in module:
            name = symbol.GetName()
            if name is None:
                continue
            if name.startswith("v8dbg_") or name.startswith("nodedbg_"):
                if command and command not in name:
                    continue
                start = symbol.GetStartAddress()
                size = symbol.GetEndAddress().GetOffset() - start.GetOffset()

                error = lldb.SBError()
                uint = bytes(target.ReadMemory(start, size, error))
                if error.Success():
                    uint = int_from_bytearray(uint, size, target)
                    print("%s: %d" % (name, uint))
                else:
                    print('error: ', error.GetCString())

lldb.debugger.HandleCommand('command script add -f helpers.brute v8-brute-inspect')
lldb.debugger.HandleCommand('command script add -f helpers.get_constants v8-get-constants')
