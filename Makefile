all:
	@echo "Please take a look at README.md"

install-osx:
	mkdir -p ~/Library/Application\ Support/LLDB/PlugIns/
	cp -rf ./out/Release/llnode.dylib \
		~/Library/Application\ Support/LLDB/PlugIns/

uninstall-osx:
	rm ~/Library/Application\ Support/LLDB/PlugIns/llnode.dylib

install-linux:
	mkdir -p /usr/lib/lldb/plugins
	cp -rf ./out/Release/lib.target/llnode.so /usr/lib/lldb/plugins

uninstall-linux:
	rm /usr/lib/lldb/plugins/llnode.so

format:
	clang-format -i src/*

configure: scripts/configure.js
	node scripts/configure.js

plugin: configure
	./gyp_llnode
	$(MAKE) -C out/

_travis: plugin
	TEST_LLDB_BINARY=`which lldb-3.9` TEST_LLNODE_DEBUG=true npm test

.PHONY: all
