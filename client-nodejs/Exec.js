
var async = require('async');
var fs = require('fs');
var net = require('net');
var d3b = require('./d3b');

function doRequest(client, opcode, request) {
	var req = client.request();
	req.send(opcode, request);
	console.log("[" + req.seqNumber + "] > "
		+ opcode + ": " + JSON.stringify(request));
	req.on('response', function(resp_opcode, response) {
		console.log("[" + req.seqNumber + "] < "
				+ resp_opcode + ": " + JSON.stringify(response,
			function(key, value) {
				if(Buffer.isBuffer(value)) {
					return "[buffer:" + value.toString() + "]"
				}else return value;
			}));
	});
}

if(process.argv.length != 4)
	throw new Error("Expected exactly two arguments");

console.log("[info] Connecting...");
var client = new d3b.Client();
client.connect(7963, 'localhost', fs.readFileSync('server.crt'));
client.on('connect', function() {
	console.log("[info] Connected!");
	var opcode = d3b.ClientRequests[process.argv[2]];
	if(!opcode)
		throw new Error("[error] Illegal opcode " + process.argv[2]);
	var data = process.argv[3];
	try {
		var request = JSON.parse(data);
		doRequest(client, opcode, request);
	}catch(err) {
		if(err instanceof SyntaxError) {
			console.log("[error] Could not parse string '"
					+ data + "' (" + err + ")");
		}else throw err;
	}
});
client.on('end', function() {
	console.log("[info] Disconnect");
});

