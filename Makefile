.PHONY: all
all: configure-with-addon
	node-gyp rebuild
	node scripts/cleanup.js

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
	npm run format

# This depends on the system setting e.g. $PATH so can't actually be skipped
.PHONY: configure
configure:
	node scripts/configure.js

.PHONY: configure-with-addon
configure-with-addon:
	LLNODE_BUILD_ADDON=true node scripts/configure.js

.PHONY: plugin
plugin: configure
	node-gyp rebuild
	node scripts/cleanup.js

.PHONY: addon
addon: configure-with-addon
	node-gyp rebuild

.PHONY: coverage
coverage:
	lcov --capture --directory build/ --output-file coverage-cc.info
	lcov --remove coverage-cc.info "/usr/*" "*/deps/*" "*/node_modules/*" --output-file coverage-cc.info # filter system-files
	lcov --list coverage-cc.info

.PHONY: clean
clean:
	$(RM) -r build
	$(RM) config.gypi
	$(RM) lldb
	$(RM) llnode.so llnode.dylib
