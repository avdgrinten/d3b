
d := shard

OBJECTS = main.o db/engine.o db/storage-driver.o \
	db/view-driver.o db/flex-storage.o db/js-view.o \
	ll/write-ahead.o ll/page-cache.o ll/random-access-file.o \
	ll/tasks.o ll/crypto.o ll/tls.o \
	api/server.o proto/Api.o \
	proto/Config.o os/linux.o

DIRS = db ll api proto os

V8_PATH = $(HOME)/v8

CXXFLAGS += -std=c++14 -pthread -I$d/include
CXXFLAGS += -I$(V8_PATH)/include

LIBS += -lprotobuf-lite -lboost_program_options
LIBS += -L$(V8_PATH)/out/x64.release/obj.target/src
LIBS += -L$(V8_PATH)/out/x64.release/obj.target/third_party/icu
LIBS += -Wl,--start-group -lv8_base -lv8_libbase -lv8_external_snapshot -lv8_libplatform \
	-licuuc -licui18n -licudata -Wl,--end-group
LIBS += -lbotan-1.11
LIBS += -ldl

.PHONY: all
all: $d/bin/shard

.PHONY: clean
clean: d := $d
clean:
	rm -rf $d/obj/ $d/bin/

$d/obj $(addprefix $d/obj/,$(DIRS)) $d/bin:
	mkdir -p $@

define compile_cxx =
@echo '(CXX) -c -o $@'
@$(CXX) -c -o $@ $(CXXFLAGS) -MD $<
endef

$d/obj/%.o: d := $d
$d/obj/%.o: CXXFLAGS += -O3

$d/obj/%.o: $d/src/%.cpp | $d/obj $(addprefix $d/obj/,$(DIRS))
	$(compile_cxx)

$d/obj/%.o: $d/src/%.pb.cc | $d/obj $(addprefix $d/obj/,$(DIRS))
	$(compile_cxx)

$d/bin/shard: d := $d
$d/bin/shard: $(addprefix $d/obj/,$(OBJECTS)) | $d/bin
	@echo '(CXX) -o $@'
	@$(CXX) -o $@ $(CXXFLAGS) $(addprefix $d/obj/,$(OBJECTS)) $(LIBS)

-include $(addprefix $d/obj/,$(OBJECTS:%.o=%.d))

d :=

