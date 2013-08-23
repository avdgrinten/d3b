
var async = require('async');
var common = require('./common');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

setUp: function(callback) {
	this.instance = new common.D3bInstance();

	var self = this;
	self.instance.setup(callback);
},

tearDown: function(callback) {
	this.instance.shutdown(callback);
},

testInsert: function(test) {
	var data = [ ];
	for(var i = 0; i < 1000; i++) {
		data.push('item #' + i);
	}

	var self = this;
	async.series([
		function(callback) {
			self.instance.connect(function(client) {
				self.client = client;
				callback();
			});
		},
		function(callback) {
			var file = require('fs').readFileSync('tests/views/simple-view.js');
			d3bUtil.uploadExtern(self.client, { fileName: 'simple-view.js',
					buffer: file }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.createStorage(self.client, { driver: 'FlexStorage',
					identifier: 'test-storage' }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.createView(self.client, { driver: 'JsView',
					identifier: 'test-view', baseStorage: 'test-storage',
					scriptFile: 'simple-view.js' }, function() {
				callback();
			});
		},
		function(callback) {
			async.each(data, function(item, callback) {
				d3bUtil.insert(self.client, { storageName: 'test-storage',
						buffer: item }, function() {
					callback();
				});
			}, callback);
		},
		function(callback) {
			var queue = [ ];
			d3bUtil.query(self.client, { viewName: 'test-view' },
				function(row) {
					queue.push(JSON.parse(row).id);
				}, function() {
					async.each(queue, function(id, callback) {
						d3bUtil.update(self.client, { storageName: 'test-storage',
								id: id, buffer: 'modified-' + id }, function() {
							callback();
						});
					}, callback);
				});
		},
		function(callback) {
			var retrieved = [ ];
			d3bUtil.query(self.client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row));
				}, function() {
					test.equal(retrieved.length, data.length);
					for(var i = 0; i < data.length; i++)
						test.equal(retrieved[i].buffer, 'modified-' + retrieved[i].id);
					callback();
				});
		},
		function(callback) {
			self.client.close(callback);
		}
	], function() {
		test.done();
	});
},

};

