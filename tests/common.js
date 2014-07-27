
var fs = require('fs');
var net = require('net');
var child_process = require('child_process');
var async = require('async');
var d3b = require('../client-nodejs/d3b');

var showServerOutput = !!process.env.D3B_SHOW_SERVER_OUTPUT;
var useRunningServer = !!process.env.D3B_USE_RUNNING_SERVER;

process.on('uncaughtException', function(error) {
	console.log("Uncaught exception:", error.stack);
});

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
	if(useRunningServer)
		return callback();
	
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
			var opts = { stdio: 'ignore' };
			if(showServerOutput)
				opts.stdio = 'inherit';
			self.$process = child_process.spawn('shard/shard',
				[ '--path', self.$path ], opts);
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
	if(useRunningServer)
		return callback();

	var self = this;
	setTimeout(function() {
		if(self.$exited) {
			cleanPath(self.$path, callback);
		}else{
			self.$process.on('exit', function() {
				cleanPath(self.$path, callback);
			});
			self.$process.kill();
		}
	}, 100);
};
D3bInstance.prototype.connect = function(callback) {
	var client = new d3b.Client();
	client.on('connect', function() {
		callback(client);
	});
	client.on('end', function() {
	});

	var server_cert = fs.readFileSync('server.crt');
	client.connect(7963, 'localhost', server_cert);
};

module.exports.D3bInstance = D3bInstance;

