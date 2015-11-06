CXXFLAGS ?=
LDFLAGS ?=
LLDB_FOLDER ?=
LLDB_BUILD_DIR ?= /Applications/Xcode.app/Contents/SharedFrameworks

CXXFLAGS += -g -O0 -Wall -Wextra -Wno-unused-parameter -std=c++11
CXXFLAGS += -I. -I$(LLDB_FOLDER)/include

LDFLAGS += -F$(LLDB_BUILD_DIR) -Wl,-rpath,"$(LLDB_BUILD_DIR)" -framework LLDB

SRC += src/llnode.cc
SRC += src/llv8.cc
HEADERS += src/llnode.h
HEADERS += src/llv8.h
HEADERS += src/llv8-inl.h

llnode.dylib: $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -dynamiclib $(SRC) -o $@ $(LDFLAGS)
