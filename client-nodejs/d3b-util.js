
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
		driver: opts.driver, identifier: opts.identifier,
			config: { } });
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
		driver: opts.driver, identifier: opts.identifier,
		config: { baseStorage: opts.baseStorage,
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

function uploadExtern(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqUploadExtern, {
		fileName: opts.fileName, buffer: opts.buffer });
}

function downloadExtern(client, opts, data_handler, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(null);
			req.fin();
		}else if(opcode == d3b.ServerResponses.kSrBlob) {
			data_handler(resp.buffer);
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqDownloadExtern, {
		fileName: opts.fileName });
}

function insert(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			var result = { sequenceId: parseInt(resp.sequenceId),
				documentId: resp.mutations[0].documentId };
			callback(resp.error, result);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqShortTransact, {
		mutations: [ { type: 'kTypeInsert', storageName: opts.storageName,
			buffer: opts.buffer } ] });
}

function modify(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			var result = { sequenceId: parseInt(resp.sequenceId) };
			callback(resp.error, result);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	req.send(d3b.ClientRequests.kCqShortTransact, {
		mutations: [ { type: 'kTypeModify', storageName: opts.storageName,
			documentId: opts.id, buffer: opts.buffer } ],
		constraints: [ { type: 'kTypeDocumentState', storageName: opts.storageName,
			documentId: opts.id, mustExist: true } ] });
}

function query(client, opts, row_handler, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(resp.error);
			req.fin();
		}else if(opcode == d3b.ServerResponses.kSrRows) {
			resp.rowData.forEach(row_handler);
		}else throw new Error("Unexpected response " + opcode);
	});
	var message = { viewName: opts.viewName };
	if(opts.sequenceId)
		message.sequenceId = opts.sequenceId;
	req.send(d3b.ClientRequests.kCqQuery, message);
}

function transaction(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(resp.error, resp.transactionId);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	var message = { };
	req.send(d3b.ClientRequests.kCqTransaction, message);
}

function update(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(resp.error);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	var message = { transactionId: opts.transactionId,
			mutations: opts.mutations,
			constraints: opts.constraints };
	req.send(d3b.ClientRequests.kCqUpdate, message);
}

function apply(client, opts, callback) {
	var req = client.request();
	req.on('response', function(opcode, resp) {
		if(opcode == d3b.ServerResponses.kSrFin) {
			callback(resp.error);
			req.fin();
		}else throw new Error("Unexpected response " + opcode);
	});
	var message = { transactionId: opts.transactionId, type: opts.type };
	req.send(d3b.ClientRequests.kCqApply, message);
}

function shutdown(client) {
	var req = client.request();
	req.send(d3b.ClientRequests.kCqShutdown, { });
}

module.exports.createStorage = createStorage;
module.exports.createView = createView;
module.exports.unlinkStorage = unlinkStorage;
module.exports.unlinkView = unlinkView;
module.exports.uploadExtern = uploadExtern;
module.exports.downloadExtern = downloadExtern;
module.exports.insert = insert;
module.exports.modify = modify;
module.exports.query = query;
module.exports.transaction = transaction;
module.exports.update = update;
module.exports.apply = apply;
module.exports.shutdown = shutdown;

