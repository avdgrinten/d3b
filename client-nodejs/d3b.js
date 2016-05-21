
'use strict';

var events = require('events');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var api = require('./Api_pb.js');

var dirname = require('path').dirname(module.filename);

var ClientRequests = {
	kCqFetch: 1,
	kCqQuery: 2,
	kCqShortTransact: 3,
	kCqTransaction: 4,
	kCqUpdate: 5,
	kCqApply: 6,

	kCqCreateStorage: 256,
	kCqCreateView: 257,
	kCqUnlinkStorage: 258,
	kCqUnlinkView: 259,
	kCqUploadExtern: 260,
	kCqDownloadExtern: 261,
	kCqShutdown: 262
};
var ServerResponses = {
	kSrFin: 1,
	kSrRows: 2,
	kSrBlob: 3,
	kSrShortTransact: 4
};

function Client() {
	this.p_socket = null;
	
	this.headBuffer = new Buffer(12);
	this.bodyBuffer = null;
	this.headBytes = 0;
	this.bodyBytes = 0;
	this.state = 'head';

	this.packetOpcode = null;
	this.packetLength = null;
	this.packetSeqNumber = null;

	this.sequence = 1;
	this.activeRequests = { };

	events.EventEmitter.call(this);
}
require('util').inherits(Client, events.EventEmitter);

Client.prototype.connect = function(port, host, server_cert) {
	this.p_socket = tls.connect(port, host, { ca: [ server_cert ] });
	this.p_socket.on('secureConnect', function() {
		this.emit('connect');
	}.bind(this));
	this.p_socket.on('data', function(buffer) {
		this.p_onPartial(buffer);
	}.bind(this));
	this.p_socket.on('end', function() {
		this.emit('end');
	}.bind(this));
}
Client.prototype.close = function(callback) {
	this.p_socket.end();
	callback();
}

Client.prototype.p_onMessage = function() {
//	console.log("msg");
	var Message;
	switch(this.packetOpcode) {
	case ServerResponses.kSrFin: Message = api.SrFin; break;
	case ServerResponses.kSrRows: Message = api.SrRows; break;
	case ServerResponses.kSrBlob: Message = api.SrBlob; break;
	case ServerResponses.kSrShortTransact: Message = api.SrShortTransact; break;
	default: throw new Error("p_onMessage(): Illegal opcode");
	}
	
	var array_buffer = new ArrayBuffer(this.bodyBuffer.length);
	var view = new Uint8Array(array_buffer);
	for (var i = 0; i < this.bodyBuffer.length; ++i)
		view[i] = this.bodyBuffer[i];
	
	var resp = Message.deserializeBinary(array_buffer);
	
	if(this.debug)
		console.log('[' + this.packetSeqNumber + '] < ' + this.packetOpcode
			+ ' ' + JSON.stringify(resp));

	if(this.packetSeqNumber in this.activeRequests) {
		var request = this.activeRequests[this.packetSeqNumber];
		request.emit('response', this.packetOpcode, resp);
	}else console.log("Lost response: " + JSON.stringify(resp));
}
Client.prototype.p_send = function(opcode, seq_number, request) {
	if(this.debug)
		console.log('[' + seq_number + '] < ' + opcode + ' ' + request);
	
	var serialized = request.serializeBinary();

	var header = new Buffer(12);
	header.writeUInt32LE(opcode, 0);
	header.writeUInt32LE(serialized.length, 4);
	header.writeUInt32LE(seq_number, 8);

	this.p_socket.write(header);
	this.p_socket.write(Buffer.from(serialized));
}
Client.prototype.p_onPartial = function(buffer) {
//	console.log("recv");
	var i = 0;
	while(i < buffer.length) {
		if(this.state == 'head') {
			var bytes = 12 - this.headBytes;
			if(bytes > buffer.length - i)
				bytes = buffer.length - i;
			buffer.copy(this.headBuffer, this.headBytes, i, i + bytes);
			i += bytes;
			this.headBytes += bytes;
	
			if(this.headBytes == 12) {
				this.packetOpcode = this.headBuffer.readUInt32LE(0);
				this.packetLength = this.headBuffer.readUInt32LE(4);
				this.packetSeqNumber = this.headBuffer.readUInt32LE(8);
//				console.log("head: " + this.packetOpcode + ", len: " + this.packetLength
//						+ ", seq: " + this.packetSeqNumber);

				this.bodyBuffer = new Buffer(this.packetLength);
				this.bodyBytes = 0;
				this.state = 'body';
			}
		}
		if(this.state == 'body') {
			var bytes = this.packetLength - this.bodyBytes;
			if(bytes > buffer.length - i)
				bytes = buffer.length - i;
			buffer.copy(this.bodyBuffer, this.bodyBytes, i, i + bytes);
			i += bytes;
			this.bodyBytes += bytes;
			
			if(this.bodyBytes == this.packetLength) {
				this.p_onMessage();
				this.headBytes = 0;
				this.state = 'head';
			}
		}
	}
};
Client.prototype.request = function() {
	return new Request(this);
};

function Request(client) {
	this.client = client;
	
	this.seqNumber = this.client.sequence;
	this.client.sequence++;
	this.client.activeRequests[this.seqNumber] = this;
	
	events.EventEmitter.call(this);
}
require('util').inherits(Request, events.EventEmitter);

Request.prototype.send = function(opcode, request) {
	this.client.p_send(opcode, this.seqNumber, request);
}
Request.prototype.fin = function() {
	delete this.client.activeRequests[this.seqNumber];
}

module.exports = {
	ClientRequests: ClientRequests,
	ServerResponses: ServerResponses,
	Client: Client
};

