
var net = require('net');
var child_process = require('child_process');
var async = require('async');
var d3b = require('../client-nodejs/d3b');

function setupPath(path, callback) {
	child_process.exec('mkdir ' + path, { },
			function(error, stdout, stderr) {
		if(error)
			throw new Error("mkdir failed");
		callback();
	});
}
function cleanPath(path, callback) {
	child_process.exec('rm -rf ' + path, { },
			function() {
		callback();
	});
}

function D3bInstance() {

}
D3bInstance.prototype.setup = function(callback) {
	this.$path = 'testdb';

	var self = this;
	async.series([
		function(callback) {
			cleanPath(self.$path, callback);
		},
		function(callback) {
			setupPath(self.$path, callback);
		},
		function(callback) {
			self.$process = child_process.spawn('shard/shard',
				[ '--create', '--path', self.$path ]);
			self.$process.on('exit', function(code) {
				callback();
			});
		},
		function(callback) {
			self.$process = child_process.spawn('shard/shard',
				[ '--path', self.$path ] /*, { stdio: 'inherit' }*/);
			self.$process.on('exit', function() {
				self.$exited = true;
			});
			callback();
		},
		function(callback) {
			setTimeout(callback, 100);
		}
	], callback);
};
D3bInstance.prototype.shutdown = function(callback) {
	if(this.$exited) {
		cleanPath(this.$path, callback);
	}else{
		var self = this;
		this.$process.on('exit', function() {
			cleanPath(self.$path, callback);
		});
		this.$process.kill();
	}
};
D3bInstance.prototype.connect = function(callback) {
	var client = new d3b.Client();
	client.on('connect', function() {
		callback(client);
	});
	client.on('end', function() {
	});
	client.useSocket(net.connect(7963, 'localhost'));
};

module.exports.D3bInstance = D3bInstance;

