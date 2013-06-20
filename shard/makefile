
debug:
	g++ -o shard -g -std=c++0x -I include/ src/Database.cpp src/Db/Engine.cpp src/Db/Transact.cpp src/Db/FlexImutStorage.cpp src/Db/JsView.cpp src/Ll/DataStore.cpp src/Api/Server.cpp src/proto/Api.pb.cc src/proto/Config.pb.cc src/proto/Request.pb.cc src/Os/Linux.cpp -lv8 -lprotobuf-lite -lboost_program_options

opt:
	g++ -o shard -O3 -std=c++0x -I include/ src/Database.cpp src/Db/Engine.cpp src/Db/Transact.cpp src/Db/FlexImutStorage.cpp src/Db/JsView.cpp src/Ll/DataStore.cpp src/Api/Server.cpp src/proto/Api.pb.cc src/proto/Config.pb.cc src/proto/Request.pb.cc src/Os/Linux.cpp -lv8 -lprotobuf-lite -lboost_program_options
