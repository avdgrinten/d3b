
const assert = require('assert');

const d3b = require('./d3b');
const api = require('./Api_pb.js');

function nodeToView(node_buffer) {
	assert(Buffer.isBuffer(node_buffer));
	return new Uint8Array(node_buffer.buffer,
			node_buffer.byteOffset, node_buffer.byteLength);
};
function viewToNode(view) {
	assert(view instanceof Uint8Array);
	return Buffer.from(view, view.byteOffset, view.byteLength);
};

function createStorage(client, opts, callback) {
	let req = new api.CqCreateStorage();
	req.setDriver(opts.driver);
	req.setIdentifier(opts.identifier);

	return new Promise((resolve, reject) => {
		let exchange = client.exchange((opcode, resp) => {
			if(opcode == d3b.ServerResponses.kSrFin) {
				resolve();
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqCreateStorage, req);
	});
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

function uploadExtern(client, opts) {
	return new Promise((resolve, reject) => {
		var req = new api.CqUploadExtern();
		req.setFileName(opts.fileName);
		req.setBuffer(nodeToView(opts.buffer));

		var exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrFin) {
				resolve();
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqUploadExtern, req);
	});
}

function downloadExtern(client, opts) {
	let file;

	return new Promise((resolve, reject) => {
		var req = new api.CqDownloadExtern();
		req.setFileName(opts.fileName);

		var exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrBlob) {
				file = data.getBuffer();
			}else if(opcode == d3b.ServerResponses.kSrFin) {
				assert(file);
				resolve(viewToNode(file));
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqDownloadExtern, req);
	});
}

function insert(client, opts) {
	return new Promise((resolve, reject) => {
		let mutation = new api.Mutation();
		mutation.setType(api.Mutation.Type.KTYPEINSERT);
		mutation.setStorageName(opts.storageName);
		mutation.setBuffer(nodeToView(opts.buffer));

		let req = new api.CqShortTransact();
		req.setMutationsList([ mutation ]);

		let exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					resolve({
						sequenceId: data.getSequenceId(),
						documentId: data.getMutationsList()[0].getDocumentId()
					});
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqShortTransact, req);
	});
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

function fetch(client, opts, on_data, callback) {
	let result;

	return new Promise((resolve, reject) => {
		let req = new api.CqFetch();
		req.setStorageName(opts.storageName);
		req.setDocumentId(opts.documentId);
		if(opts.sequenceId)
			req.setSequenceId(opts.sequenceId);

		let exchange = client.exchange(function(opcode, data) {
			if(opcode == d3b.ServerResponses.kSrBlob) {
				result = data.getBuffer();
			}else if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					assert(result);
					resolve(viewToNode(result));
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqFetch, req);
	});
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
module.exports.fetch = fetch;
module.exports.query = query;
module.exports.transaction = transaction;
module.exports.update = update;
module.exports.apply = apply;
module.exports.shutdown = shutdown;

