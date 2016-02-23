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

_travis:
	./gyp_llnode -Dlldb_dir=/usr/lib/llvm-3.6/ -Dlldb_lib=lldb-3.6 -f make
	make -C out/
	TEST_LLDB_BINARY=`which lldb-3.6` npm test

.PHONY: all
