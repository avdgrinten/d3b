
hook("extractDoc", function(id, buffer) {
	var doc = { id: id, buffer: buffer };
	return doc;
});

hook("extractKey", function(buffer) {
	return parseInt(buffer);
});

hook("keyOf", function(doc) {
	return doc.id;
});

hook("compare", function(a, b) {
        if(a < b)
                return -1;
        if(a > b)
                return 1;
        return 0;
});

hook("serializeKey", function(key) {
	return key.toString();
});
hook("deserializeKey", function(buffer) {
	return parseInt(buffer);
});

hook("report", function(doc) {
	return JSON.stringify(doc);
});

