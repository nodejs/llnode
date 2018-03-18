
# Usage

<!-- TOC -->

- [Loading the llnode Plugin](#loading-the-llnode-plugin)
  - [1. Using the llnode shortcut](#1-using-the-llnode-shortcut)
  - [2. Using `~/.lldbinit` to load the Plugin Automatically](#2-using-lldbinit-to-load-the-plugin-automatically)
  - [3. Loading the Plugin Manually](#3-loading-the-plugin-manually)
  - [4. Install the Plugin to the LLDB System Plugin Directory](#4-install-the-plugin-to-the-lldb-system-plugin-directory)
- [Starting the command line](#starting-the-command-line)
- [Commands](#commands)

<!-- /TOC -->

## Loading the llnode Plugin

There are several ways to load the llnode plugin:

### 1. Using the llnode shortcut

If you install llnode globally via npm (`npm install -g llnode`), you can use
the `llnode` shortcut script. This starts `lldb` and automatically issues
the `plugin load` command. All parameters to the llnode script are passed
directly to lldb. If you it's not a local installation, the shortcut will be in
`node_modules/.bin/llnode`.

### 2. Using `~/.lldbinit` to load the Plugin Automatically

To tell LLDB to load llnode automatically regardless of the
version of lldb that you are running, add this line to `~/.lldbinit`:

```
plugin load /path/to/the/llnode/plugin
```

The path to the llnode plugin should be printed when the installation
is finished. On OS X/macOS the plugin is typically
`node_modules/llnode/llnode.dylib`, on Linux it's
`node_modules/llnode/llnode.so`.

### 3. Loading the Plugin Manually

The llnode plugin can also be manually loaded into LLDB using the
`plugin load` command within lldb.

It does not matter whether the `plugin load` command is issued before or after
loading a core dump or attaching to a process.

### 4. Install the Plugin to the LLDB System Plugin Directory

Similar to the `~/.lldbinit` approach, this way LLDB will also load the plugin
automatically on start-up. Doing this may require additional permissions
to be able to copy the plugin libary to the system plugin directory.

On Linux, run `make install-linux` in the project directory, or if
installing with npm, copy `node_modules/llnode/llnode.so`
to `/usr/lib/lldb/plugins` or create a link there.

On OS X/macOS, run `make install-osx` in the project directory, or if
installing with npm, copy `node_modules/llnode/llnode.dylib`
to `~/Library/Application\ Support/LLDB/PlugIns/` or create a link there.

## Starting the command line

To use llnode with a core dump the core dump needs to be loaded into lldb
along with the exact executable that created the core dump. The executable
contains information that lldb and the llnode plugin need to make sense of
the data in the core dump.

To load a core dump when starting llnode use:

```
llnode /path/to/bin/node -c /path/to/core
```

or to load the core dump after starting lldb:

```
(llnode) target create /path/to/bin/node -c /path/to/core
```

To use llnode against a live process:

```
llnode -- /path/to/bin/node script.js
(llnode) run
```

This is ideal for debugging an npm package with native code.
To debug a Node.js crash on uncaught exception:

```
llnode -- /path/to/bin/node --abort_on_uncaught_exception script.js
(llnode) run
```

lldb will stop your process when it crashes. To see where it stopped use the
v8 bt command. See the [Commands](#commands) section below for more commands.

## Commands

```
(llnode) v8 help
     Node.js helpers

Syntax: v8

The following subcommands are supported:

      bt              -- Show a backtrace with node.js JavaScript functions and their args. An optional argument is accepted; if
                         that argument is a number, it specifies the number of frames to display. Otherwise all frames will be
                         dumped.

                         Syntax: v8 bt [number]
      findjsinstances -- List every object with the specified type name.
                         Use -v or --verbose to display detailed `v8 inspect` output for each object.
                         Accepts the same options as `v8 inspect`
      findjsobjects   -- List all object types and instance counts grouped by typename and sorted by instance count. Use
                         -d or --detailed to get an output grouped by type name, properties, and array length, as well as
                         more information regarding each type.
                         With lldb < 3.9, requires the `LLNODE_RANGESFILE` environment variable to be set to a file
                         containing memory ranges for the core file being debugged.
                         There are scripts for generating this file on Linux and Mac in the scripts directory of the llnode
                         repository.
      findrefs        -- Finds all the object properties which meet the search criteria.
                         The default is to list all the object properties that reference the specified value.
                         Flags:

                          * -v, --value expr     - all properties that refer to the specified JavaScript object (default)
                          * -n, --name  name     - all properties with the specified name
                          * -s, --string string  - all properties that refer to the specified JavaScript string value

      inspect         -- Print detailed description and contents of the JavaScript value.

                         Possible flags (all optional):

                          * -F, --full-string    - print whole string without adding ellipsis
                          * -m, --print-map      - print object's map address
                          * -s, --print-source   - print source code for function objects
                          * -l num, --length num - print maximum of `num` elements from string/array

                         Syntax: v8 inspect [flags] expr
      nodeinfo        -- Print information about Node.js
      print           -- Print short description of the JavaScript value.

                         Syntax: v8 print expr
      source          -- Source code information

For more help on any particular subcommand, type 'help <command> <subcommand>'.
```
