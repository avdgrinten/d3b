
OBJECTS = Database.o Db/Engine.o Db/StorageDriver.o \
	Db/ViewDriver.o Db/FlexStorage.o Db/JsView.o \
	Ll/WriteAhead.o \
	Ll/PageCache.o \
	Ll/RandomAccessFile.o \
	Ll/Tasks.o \
	Ll/Crypto.o \
	Ll/Tls.o \
	Api/Server.o proto/Api.o \
	proto/Config.o Os/Linux.o

DIRS = Db Ll Api proto Os

V8_PATH = $(HOME)/v8

CXXFLAGS += -std=c++14 -pthread -Iinclude
CXXFLAGS += -I$(V8_PATH)/include

LIBS += -lprotobuf-lite -lboost_program_options
LIBS += -L$(V8_PATH)/out/x64.release/obj.target/src
LIBS += -L$(V8_PATH)/out/x64.release/obj.target/third_party/icu
LIBS += -Wl,--start-group -lv8_base -lv8_libbase -lv8_external_snapshot -lv8_libplatform \
	-licuuc -licui18n -licudata -Wl,--end-group
LIBS += -lbotan-1.11
LIBS += -ldl

.PHONY: all
all: bin/shard

.PHONY: clean
clean:
	rm -rf obj/ bin/

obj $(addprefix obj/,$(DIRS)) bin:
	mkdir -p $@

compile_cxx = $(CXX) -c -o $@ $(CXXFLAGS) -MD $<

obj/%.o: CXXFLAGS += -O3

obj/%.o: src/%.cpp | obj $(addprefix obj/,$(DIRS))
	$(compile_cxx)

obj/%.o: src/%.pb.cc | obj $(addprefix obj/,$(DIRS))
	$(compile_cxx)

bin/shard: $(addprefix obj/,$(OBJECTS)) | bin
	$(CXX) -o $@ $(CXXFLAGS) $(addprefix obj/,$(OBJECTS)) $(LIBS)

-include $(addprefix obj/,$(OBJECTS:%.o=%.d))

