
var d3b = require('./d3b');

function createStorage(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqCreateStorage, {
		config: { driver: opts.driver, identifier: opts.identifier } });
}

function createView(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqCreateView, {
		config: { driver: opts.driver, identifier: opts.identifier,
			baseStorage: opts.baseStorage,
			scriptFile: opts.scriptFile } });
}

function unlinkStorage(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqUnlinkStorage, {
		identifier: opts.identifier });
}

function unlinkView(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqUnlinkView, {
		identifier: opts.identifier });
}

function insert(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqShortTransact, {
		updates: [ { action: 'kActInsert', storageName: opts.storageName,
			buffer: opts.buffer } ] });
}

function update(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqShortTransact, {
		updates: [ { action: 'kActUpdate', storageName: opts.storageName,
			id: opts.id, buffer: opts.buffer } ] });
}

function query(client, opts, row_handler, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else if(opcode == d3b.ServerResponses.kSrRows) {
			resp.rowData.forEach(row_handler);
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqQuery, {
		query: { viewName: opts.viewName } });
}

module.exports.createStorage = createStorage;
module.exports.createView = createView;
module.exports.unlinkStorage = unlinkStorage;
module.exports.unlinkView = unlinkView;
module.exports.insert = insert;
module.exports.update = update;
module.exports.query = query;

