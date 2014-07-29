

var async = require('async');
var common = require('./common');
var d3b = require('../client-nodejs/d3b');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testConstraintViolation: function(test) {
	var instance, client;

	async.series([
		function(callback) {
			instance = new common.D3bInstance();
			instance.setup(callback);
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
				callback();
			});
		},
		function(callback) {
			d3bUtil.createStorage(client, { driver: 'FlexStorage',
					identifier: 'test-storage' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.insert(client, { storageName: 'test-storage',
					buffer: "Hello world!" }, function() {
				callback();
			});
		},
		function(callback) {
			var req = client.request();
			req.on('response', function(opcode, resp) {
				if(opcode == d3b.ServerResponses.kSrFin) {
					callback();
					req.fin();
				}else throw new Error("Unexpected response " + opcode);
			});
			req.send(d3b.ClientRequests.kCqShortTransact, {
				constraints: [ { type: 'kTypeDocumentState', storageName: 'test-storage',
					documentId: 2, sequenceId: 100 } ] });
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.shutdown(callback);
		}
	], function() {
		test.done();
	});
},

testConstraintConflict: function(test) {
	var instance, client;

	var transaction_id1 = null;
	var transaction_id2 = null;

	async.series([
		function(callback) {
			instance = new common.D3bInstance();
			instance.setup(callback);
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
				callback();
			});
		},
		function(callback) {
			d3bUtil.createStorage(client, { driver: 'FlexStorage',
					identifier: 'test-storage' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.insert(client, { storageName: 'test-storage',
					buffer: "Hello world!" }, function() {
				callback();
			});
		},
		// submit a first transaction that modifies the document
		function(callback) {
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id1 = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id1,
					mutations: [ { type: 'kTypeModify', storageName: 'test-storage',
						documentId: 1, buffer: 'Some modification' } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id1, doSubmit: true },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		// submit a second transaction that depends on the document's state
		function(callback) {
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id2 = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id2,
					constraints: [ { type: 'kTypeDocumentState', storageName: 'test-storage',
						documentId: 1, matchSequenceId: true, sequenceId: 1 } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id2, doSubmit: true },
				function(error) {
					test.equal(error, 'kCodeSubmitConstraintConflict');
					callback();
				});
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.shutdown(callback);
		}
	], function() {
		test.done();
	});
},

testMutationConflict: function(test) {
	var instance, client;

	var transaction_id1 = null;
	var transaction_id2 = null;

	async.series([
		function(callback) {
			instance = new common.D3bInstance();
			instance.setup(callback);
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
				callback();
			});
		},
		function(callback) {
			d3bUtil.createStorage(client, { driver: 'FlexStorage',
					identifier: 'test-storage' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.insert(client, { storageName: 'test-storage',
					buffer: "Hello world!" }, function() {
				callback();
			});
		},
		// submit a first transaction that depends on the document's state
		function(callback) {
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id1 = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id1,
					constraints: [ { type: 'kTypeDocumentState', storageName: 'test-storage',
						documentId: 1, matchSequenceId: true, sequenceId: 1 } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id1, doSubmit: true },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		// submit a second transaction that modifies the document
		function(callback) {
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id2 = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id2,
					mutations: [ { type: 'kTypeModify', storageName: 'test-storage',
						documentId: 1, buffer: 'Some modification' } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id2, doSubmit: true },
				function(error) {
					test.equal(error, 'kCodeSubmitMutationConflict');
					callback();
				});
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.shutdown(callback);
		}
	], function() {
		test.done();
	});
},

};
