#!/bin/bash

protoc --cpp_out=. proto/Api.proto
cp proto/Api.pb.cc shard/src/proto/Api.pb.cc
cp proto/Api.pb.h shard/include/proto/Api.pb.h

protoc --cpp_out=. proto/Config.proto
cp proto/Config.pb.cc shard/src/proto/Config.pb.cc
cp proto/Config.pb.h shard/include/proto/Config.pb.h

protoc --include_imports --descriptor_set_out=proto/Api.desc proto/Api.proto
cp proto/Api.desc client-nodejs/Api.desc

