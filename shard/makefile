
CPP_FILES = src/Database.cpp src/Db/Engine.cpp src/Db/StorageDriver.cpp src/Db/FlexStorage.cpp src/Db/JsView.cpp src/Ll/DataStore.cpp src/Ll/WriteAhead.cpp src/Api/Server.cpp src/proto/Api.pb.cc src/proto/Config.pb.cc src/proto/Request.pb.cc src/Os/Linux.cpp -lv8 -lprotobuf-lite

LIBS = -lv8 -lprotobuf-lite -lboost_program_options -lssl

debug:
	g++ -o shard -g -std=c++0x -I include/ $(CPP_FILES) $(LIBS)

opt:
	g++ -o shard -O3 -std=c++0x -I include/ $(CPP_FILES) $(LIBS)
