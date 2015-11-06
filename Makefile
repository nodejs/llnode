CXXFLAGS ?=
LDFLAGS ?=
LLDB_FOLDER ?=
LLDB_BUILD_DIR ?= /Applications/Xcode.app/Contents/SharedFrameworks

CXXFLAGS += -g -O0 -Wall -Wextra -Wno-unused-parameter -std=c++11
CXXFLAGS += -I. -I$(LLDB_FOLDER)/include

LDFLAGS += -F$(LLDB_BUILD_DIR) -Wl,-rpath,"$(LLDB_BUILD_DIR)" -framework LLDB

llnode.dylib: src/llnode.cc src/llnode.h src/v8.cc src/v8.h src/v8-inl.h
	$(CXX) $(CXXFLAGS) -dynamiclib src/llnode.cc src/v8.cc -o $@ $(LDFLAGS)
