
var async = require('async');
var common = require('./common');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testRollback: function(test) {
	var instance, client;

	var transaction_id = null;

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
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id,
					mutations: [ { type: 'kTypeInsert', storageName: 'test-storage',
						buffer: 'Hello world!' } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id, type: 'kTypeSubmit' },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id, type: 'kTypeCommit' },
				function(error) {
					test.equal(error, 'kCodeSuccess');
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

testRollback: function(test) {
	var instance, client;

	var transaction_id = null;

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
			d3bUtil.transaction(client, { }, function(error, res_transaction_id) {
				test.equal(error, 'kCodeSuccess');
				transaction_id = res_transaction_id;
				callback();
			});
		},
		function(callback) {
			d3bUtil.update(client, { transactionId: transaction_id,
					mutations: [ { type: 'kTypeInsert', storageName: 'test-storage',
						buffer: 'Hello world!' } ] },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id, type: 'kTypeSubmit' },
				function(error) {
					test.equal(error, 'kCodeSuccess');
					callback();
				});
		},
		function(callback) {
			d3bUtil.apply(client, { transactionId: transaction_id, type: 'kTypeRollback' },
				function(error) {
					test.equal(error, 'kCodeSuccess');
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

