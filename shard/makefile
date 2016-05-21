
CPP_FILES = src/Database.cpp src/Db/Engine.cpp src/Db/StorageDriver.cpp \
	src/Db/ViewDriver.cpp src/Db/FlexStorage.cpp src/Db/JsView.cpp \
	src/Ll/WriteAhead.cpp \
	src/Ll/PageCache.cpp \
	src/Ll/RandomAccessFile.cpp \
	src/Ll/Tasks.cpp \
	src/Ll/Crypto.cpp \
	src/Ll/Tls.cpp \
	src/Api/Server.cpp src/proto/Api.pb.cc \
	src/proto/Config.pb.cc src/Os/Linux.cpp

V8_PATH = $(HOME)/v8

CXXFLAGS += -pthread -Iinclude
CXXFLAGS += -I $(V8_PATH)/include

LIBS += -lprotobuf-lite -lboost_program_options
LIBS += -L $(V8_PATH)/out/x64.release/obj.target/src
LIBS += -L $(V8_PATH)/out/x64.release/obj.target/third_party/icu
LIBS += -Wl,--start-group -lv8_base -lv8_libbase -lv8_external_snapshot -lv8_libplatform \
	-licuuc -licui18n -licudata -Wl,--end-group
LIBS += -lbotan-1.11
LIBS += -ldl

debug:
	g++ -o shard -g -std=c++0x $(CXXFLAGS) -pthread $(CPP_FILES) $(LIBS)

opt:
	g++ -o shard -O3 -std=c++0x $(CXXFLAGS) -pthread $(CPP_FILES) $(LIBS)

profile:
	g++ -o shard -O3 -g -pg -std=c++0x $(CXXFLAGS) -pthread $(CPP_FILES) $(LIBS)

