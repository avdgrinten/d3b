
const assert = require('assert');

const d3b = require('./d3b');
const api = require('./Api_pb.js');
const cfg = require('./Config_pb.js');

const kMutateInsert = Symbol();
const kMutateModify = Symbol();

const kApplySubmit = Symbol();
const kApplyCommit = Symbol();
const kApplyRollback = Symbol();

function nodeToView(node_buffer) {
	assert(Buffer.isBuffer(node_buffer));
	return new Uint8Array(node_buffer.buffer,
			node_buffer.byteOffset, node_buffer.byteLength);
};
function viewToNode(view) {
	assert(view instanceof Uint8Array);
	return Buffer.from(view, view.byteOffset, view.byteLength);
};

function createStorage(client, opts) {
	return new Promise((resolve, reject) => {
		let req = new api.CqCreateStorage();
		req.setDriver(opts.driver);
		req.setIdentifier(opts.identifier);

		let exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrFin) {
				resolve();
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});

		exchange.send(d3b.ClientRequests.kCqCreateStorage, req);
	});
}

function createView(client, opts) {
	return new Promise((resolve, reject) => {
		let config = new cfg.ViewConfig();
		config.setBaseStorage(opts.baseStorage);
		config.setScriptFile(opts.scriptFile);

		let req = new api.CqCreateView();
		req.setDriver(opts.driver);
		req.setIdentifier(opts.identifier);
		req.setConfig(config);

		var exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrFin) {
				resolve();
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});
		exchange.send(d3b.ClientRequests.kCqCreateView, req);
	});
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

function fetch(client, opts) {
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

function query(client, opts, handler) {
	return new Promise((resolve, reject) => {
		let req = new api.CqQuery();
		req.setViewName(opts.viewName);
		if(opts.sequenceId)
			req.setSequenceId(opts.sequenceId);

		let exchange = client.exchange((opcode, data) => {
			if(opcode == d3b.ServerResponses.kSrRows) {
				data.getRowDataList().forEach(entry => {
					handler(viewToNode(entry));
				});
			}else if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					resolve();
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});

		exchange.send(d3b.ClientRequests.kCqQuery, req);
	});
}

function transaction(client, opts) {
	return new Promise((resolve, reject) => {
		let req = new api.CqTransaction();

		var exchange = client.exchange(function(opcode, data) {
			if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					resolve(data.getTransactionId());
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});

		exchange.send(d3b.ClientRequests.kCqTransaction, req);
	});
}

function update(client, opts) {
	return new Promise((resolve, reject) => {
		let req = new api.CqUpdate();
		req.setTransactionId(opts.transactionId);

		if(opts.mutations)
			req.setMutationsList(opts.mutations.map(entry => {
				let mutation = new api.Mutation();
				
				switch(entry.type) {
				case kMutateInsert:
					mutation.setType(api.Mutation.Type.KTYPEINSERT);
					break;
				case kMutateModify:
					mutation.setType(api.Mutation.Type.KTYPEMODIFY);
					break;
				default:
					throw new Error("Unexpected mutation type");
				}

				mutation.setStorageName(entry.storageName);
				mutation.setBuffer(nodeToView(entry.buffer));
				if(entry.documentId)
					mutation.setDocumentId(entry.documentId);

				return mutation;
			}));

		assert(!req.constraints);
	/*	if(req.constraints)
			req.setMutationsList(opts.constraints.map(entry => {
				let constraint = new api.Constraint();
			});*/

		var exchange = client.exchange(function(opcode, data) {
			if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					resolve();
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});

		exchange.send(d3b.ClientRequests.kCqUpdate, req);
	});
}

function apply(client, opts) {
	return new Promise((resolve, reject) => {
		let req = new api.CqApply();
		req.setTransactionId(opts.transactionId);

		switch(opts.type) {
		case kApplySubmit:
			req.setType(api.CqApply.Type.KTYPESUBMIT);
			break;
		case kApplyCommit:
			req.setType(api.CqApply.Type.KTYPECOMMIT);
			break;
		case kApplyRollback:
			req.setType(api.CqApply.Type.KTYPEROLLBACK);
			break;
		default:
			throw new Error("Unexpected apply type");
		}

		var exchange = client.exchange(function(opcode, data) {
			if(opcode == d3b.ServerResponses.kSrFin) {
				if(data.getError() == api.ErrorCode.KCODESUCCESS) {
					resolve();
				}else{
					reject(new Error("d3b error code " + data.getError()));
				}
				exchange.fin();
			}else throw new Error("Unexpected response " + opcode);
		});

		exchange.send(d3b.ClientRequests.kCqApply, req);
	});
}

function shutdown(client) {
	var req = client.request();
	req.send(d3b.ClientRequests.kCqShutdown, { });
}

module.exports.kMutateInsert = kMutateInsert;
module.exports.kMutateModify = kMutateModify;

module.exports.kApplySubmit = kApplySubmit;
module.exports.kApplyCommit = kApplyCommit;
module.exports.kApplyRollback = kApplyRollback;

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

