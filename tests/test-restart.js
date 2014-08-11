
var async = require('async');
var common = require('./common');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testRestart: function(test) {
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
			var file = require('fs').readFileSync('tests/views/simple-view.js');
			d3bUtil.uploadExtern(client, { fileName: 'simple-view.js',
					buffer: file }, function() {
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
			d3bUtil.createView(client, { driver: 'JsView',
					identifier: 'test-view', baseStorage: 'test-storage',
					scriptFile: 'simple-view.js' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.shutdown(client);
			instance.wait(callback);
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.start(function() {
				callback();
			});
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
				callback();
			});
		},
		function(callback) {
			d3bUtil.insert(client, { storageName: 'test-storage',
					buffer: "Hello world!" }, function(error) {
				test.equal(error, 'kCodeSuccess');
				callback();
			});
		},
		function(callback) {
			var retrieved = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row).buffer);
				}, function(error) {
					test.equal(error, 'kCodeSuccess');
					test.equal(retrieved.length, 1);
					test.equal(retrieved[0], "Hello world!");
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

testDurableInsert: function(test) {
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
			var file = require('fs').readFileSync('tests/views/simple-view.js');
			d3bUtil.uploadExtern(client, { fileName: 'simple-view.js',
					buffer: file }, function() {
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
			d3bUtil.createView(client, { driver: 'JsView',
					identifier: 'test-view', baseStorage: 'test-storage',
					scriptFile: 'simple-view.js' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.insert(client, { storageName: 'test-storage',
					buffer: "Hello world!" }, function(error) {
				test.equal(error, 'kCodeSuccess');
				callback();
			});
		},
		function(callback) {
			d3bUtil.shutdown(client);
			instance.wait(callback);
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.start(function() {
				callback();
			});
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
				callback();
			});
		},
		function(callback) {
			var retrieved = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row).buffer);
				}, function(error) {
					test.equal(error, 'kCodeSuccess');
					test.equal(retrieved.length, 1);
					test.equal(retrieved[0], "Hello world!");
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

testDurableSubmit: function(test) {
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
			var file = require('fs').readFileSync('tests/views/simple-view.js');
			d3bUtil.uploadExtern(client, { fileName: 'simple-view.js',
					buffer: file }, function() {
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
			d3bUtil.createView(client, { driver: 'JsView',
					identifier: 'test-view', baseStorage: 'test-storage',
					scriptFile: 'simple-view.js' }, function() {
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
			d3bUtil.shutdown(client);
			instance.wait(callback);
		},
		function(callback) {
			client.close(callback);
		},
		function(callback) {
			instance.start(function() {
				callback();
			});
		},
		function(callback) {
			instance.connect(function(the_client) {
				client = the_client;
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
			var retrieved = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row).buffer);
				}, function(error) {
					test.equal(error, 'kCodeSuccess');
					test.equal(retrieved.length, 1);
					test.equal(retrieved[0], "Hello world!");
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

