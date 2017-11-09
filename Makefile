TEST_LLDB_BINARY ?= $(shell which lldb-3.9)

.PHONY: all
all:
	@echo "Please take a look at README.md"

.PHONY: install-osx
install-osx: plugin
	mkdir -p ~/Library/Application\ Support/LLDB/PlugIns/
	cp -rf ./llnode.dylib \
		~/Library/Application\ Support/LLDB/PlugIns/

.PHONY: uninstall-osx
uninstall-osx:
	rm ~/Library/Application\ Support/LLDB/PlugIns/llnode.dylib

.PHONY: install-linux
install-linux: plugin
	mkdir -p /usr/lib/lldb/plugins
	cp -rf ./llnode.so /usr/lib/lldb/plugins

.PHONY: uninstall-linux
uninstall-linux:
	rm /usr/lib/lldb/plugins/llnode.so

.PHONY: format
format:
	clang-format -i src/*

# This depends on the system setting e.g. $PATH so can't actually be skipped
.PHONY: configure
configure:
	node scripts/configure.js

.PHONY: plugin
plugin: configure
	node-gyp rebuild
	node scripts/cleanup.js

.PHONY: addon
addon: configure
	node-gyp rebuild

.PHONY: _travis
_travis:
	TEST_LLDB_BINARY="$(TEST_LLDB_BINARY)" \
	TEST_LLNODE_DEBUG=true \
	LLNODE_DEBUG=true \
	npm test

.PHONY: clean
clean:
	$(RM) -r build
	$(RM) config.gypi
	$(RM) lldb
	$(RM) addon.node llnode.so llnode.dylib
