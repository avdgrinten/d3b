
var async = require('async');
var common = require('./common');
var d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testConnect: function(test) {
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
			client.close(callback);
		},
		function(callback) {
			instance.shutdown(callback);
		}
	], function() {
		test.done();
	});
},

testUpload: function(test) {
	var instance, client;

	var string = Buffer.from('Hello world!');
	
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
			d3bUtil.uploadExtern(client, { fileName: 'upload.txt',
					buffer: string }, function() {
				callback();
			});
		},
		function(callback) {
			d3bUtil.downloadExtern(client, { fileName: 'upload.txt' },
					function(buffer) {
				test.equal(Buffer.from(buffer).toString(), string.toString());
			}, callback);
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
}

};

