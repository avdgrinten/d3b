
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
			async.each(data, function(item, callback) {
				d3bUtil.insert(client, { storageName: 'test-storage',
						buffer: item }, function() {
					callback();
				});
			}, callback);
		},
		function(callback) {
			var retrieved = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row).buffer);
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

