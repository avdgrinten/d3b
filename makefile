
PROTOC ?= protoc

.DEFAULT_GOAL = all

.PHONY: gen all clean

gen: gen-shard gen-client-nodejs
all: all-shard
clean: clean-shard clean-client-nodejs

include shard/dir.makefile
include client-nodejs/dir.makefile

