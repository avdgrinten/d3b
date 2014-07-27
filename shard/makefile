
CPP_FILES = src/Database.cpp src/Db/Engine.cpp src/Db/StorageDriver.cpp \
	src/Db/ViewDriver.cpp src/Db/FlexStorage.cpp src/Db/JsView.cpp \
	src/Ll/DataStore.cpp src/Ll/WriteAhead.cpp \
	src/Ll/PageCache.cpp \
	src/Ll/Tasks.cpp \
	src/Ll/Crypto.cpp \
	src/Ll/Tls.cpp \
	src/Api/Server.cpp src/proto/Api.pb.cc \
	src/proto/Config.pb.cc src/Os/Linux.cpp

INCLUDE_DIRS = -Iinclude/
LIBS = -lv8 -lprotobuf-lite -lboost_program_options -lbotan-1.11

debug:
	g++ -o shard -g -std=c++0x $(INCLUDE_DIRS) -pthread $(CPP_FILES) $(LIBS)

opt:
	g++ -o shard -O3 -std=c++0x $(INCLUDE_DIRS) -pthread $(CPP_FILES) $(LIBS)
