
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

testConnect: function(test) {
	var self = this;
	async.series([
		function(callback) {
			self.instance.connect(function(client) {
				self.client = client;
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

testUpload: function(test) {
	var string = 'Hello world!';
	
	var self = this;
	async.series([
		function(callback) {
			self.instance.connect(function(client) {
				self.client = client;
				callback();
			});
		},
		function(callback) {
			d3bUtil.uploadExtern(self.client, { fileName: 'upload.txt',
					buffer: string }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.downloadExtern(self.client, { fileName: 'upload.txt' },
					function(buffer) {
				test.equal(buffer.toString(), string);
			}, callback);
		},
		function(callback) {
			self.client.close(callback);
		}
	], function() {
		test.done();
	});
}

};

