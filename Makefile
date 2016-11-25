LLDB_VERSION ?= 3.6

all:
	@echo "Please take a look at README.md"

install-osx:
	mkdir -p ~/Library/Application\ Support/LLDB/PlugIns/
	cp -rf ./out/Release/llnode.dylib \
		~/Library/Application\ Support/LLDB/PlugIns/

uninstall-osx:
	rm ~/Library/Application\ Support/LLDB/PlugIns/llnode.dylib

install-linux:
	mkdir -p /usr/lib/lldb
	cp -rf ./out/Release/lib.target/llnode.so /usr/lib/lldb/

uninstall-linux:
	rm /usr/lib/lldb/llnode.so

format:
	clang-format -i src/*

ci:
	./gyp_llnode -Dlldb_dir=/usr/lib/llvm-$(LLDB_VERSION)/ -f make
	make -C out/
	TEST_LLDB_BINARY=`which lldb-$(LLDB_VERSION)` npm test

.PHONY: all install-osx uninstall-osx install-linux uninstall-linux format ci
