
d := client-nodejs

.PHONY: gen-$d
gen-$d: $d/Api_pb.js $d/Config_pb.js

.PHONY: clean-$d
clean-$d: d := $d
clean-$d:
	rm -f $d/Api_pb.js $d/Config_pb.js

$d/%_pb.js: d := $d
$d/%_pb.js: proto/%.proto
	$(PROTOC) --js_out=import_style=commonjs,binary:$d -Iproto $<

d :=

