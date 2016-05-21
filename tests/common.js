
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
	this.$path = 'testdb';
}
D3bInstance.prototype.setup = function(callback) {
	if(useRunningServer)
		return callback();
	
	var self = this;
	async.series([
		function(callback) {
			cleanPath(self.$path, callback);
		},
		function(callback) {
			setupPath(self.$path, callback);
		}
	], function() {
		self.start(callback);
	});
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
D3bInstance.prototype.start = function(callback) {
	this.$exited = false;

	var self = this;
	async.series([
		function(callback) {
			self.$process = child_process.spawn('shard/bin/shard',
				[ '--create', '--path', self.$path ]);
			self.$process.on('exit', function(code) {
				callback();
			});
		},
		function(callback) {
			var opts = { stdio: 'ignore' };
			if(showServerOutput)
				opts.stdio = 'inherit';
			self.$process = child_process.spawn('shard/bin/shard',
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
D3bInstance.prototype.wait = function(callback) {
	this.$process.on('exit', function() {
		callback();
	});
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

function defaultTest(functor) {
	return test => {
		var instance, client;

		async.series([
			function(callback) {
				instance = new D3bInstance();
				instance.setup(callback);
			},
			function(callback) {
				instance.connect(function(the_client) {
					client = the_client;
					callback();
				});
			},
			function(callback) {
				Promise.resolve()
				.then(() => functor(test, client))
				.catch(error => {
					test.ok(false, error.stack);
				})
				.then(() => {
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
	};
}

module.exports.defaultTest = defaultTest;

