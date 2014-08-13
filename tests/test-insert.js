
var async = require('async');
var common = require('./common');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testInsert: function(test) {
	var instance, client;

	var data = [ ];
	for(var i = 0; i < 1000; i++) {
		data.push('item #' + i);
	}

	var document_ids = [ ];

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
			async.each(data, function(item, callback) {
				d3bUtil.insert(client, { storageName: 'test-storage',
						buffer: item }, function(error, result) {
					test.equal(error, 'kCodeSuccess');
					document_ids.push(result.documentId);
					callback();
				});
			}, callback);
		},
		function(callback) {
			var retrieved = new Array(document_ids.length);
			async.times(document_ids.length, function(i, callback) {
				d3bUtil.fetch(client, { storageName: 'test-storage',
						documentId: document_ids[i] },
					function(data) {
						retrieved[i] = data.buffer;
					}, function(error) {
						test.equal(error, 'kCodeSuccess');
						callback();
					});
			}, function() {
				test.equal(retrieved.length, data.length);
				for(var i = 0; i < data.length; i++)
					test.equal(retrieved[i], data[i]);
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

