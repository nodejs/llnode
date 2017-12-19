TEST_LLDB_BINARY ?= $(shell which lldb-3.9)

.PHONY: all
all:
	@echo "Please take a look at README.md"

.PHONY: install-osx
install-osx:
	mkdir -p ~/Library/Application\ Support/LLDB/PlugIns/
	cp -rf ./out/Release/llnode.dylib \
		~/Library/Application\ Support/LLDB/PlugIns/

.PHONY: uninstall-osx
uninstall-osx:
	rm ~/Library/Application\ Support/LLDB/PlugIns/llnode.dylib

.PHONY: install-linux
install-linux:
	mkdir -p /usr/lib/lldb/plugins
	cp -rf ./out/Release/lib.target/llnode.so /usr/lib/lldb/plugins

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
	./gyp_llnode

.PHONY: plugin
plugin: configure
	$(MAKE) -C out/
	node scripts/cleanup.js

.PHONY: _travis
_travis:
	TEST_LLDB_BINARY="$(TEST_LLDB_BINARY)" \
	TEST_LLNODE_DEBUG=true \
	LLNODE_DEBUG=true \
	npm test

.PHONY: clean
clean:
	$(RM) -r out
	$(RM) options.gypi
	$(RM) lldb
	$(RM) llnode.so llnode.dylib
