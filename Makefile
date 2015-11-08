all:
	@echo "Please take a look at README.md"

install-osx:
	mkdir -p ~/Library/Application\ Support/LLDB/PlugIns/
	cp -rf ./out/Release/llnode.dylib \
		~/Library/Application\ Support/LLDB/PlugIns/

uninstall-osx:
	rm ~/Library/Application\ Support/LLDB/PlugIns/llnode.dylib

install-linux:
	mkdir -p ~/.local/share/lldb
	cp -rf ./out/Release/lib.target/llnode.so ~/.local/share/lldb/

uninstall-linux:
	rm ./out/Release/lib.target/llnode.so ~/.local/share/lldb/llnode.so

.PHONY: all
