
#include <cstring>
#include <iostream>
#include <openssl/md5.h>

#include "Os/Linux.h"

#include "proto/Request.pb.h"

#include "Ll/WriteAhead.hpp"

Ll::WriteAhead::WriteAhead() {
	p_curSequence = 1;
	p_file = osIntf->createFile();
}

void Ll::WriteAhead::setPath(const std::string &path) {
	p_path = path;
}
void Ll::WriteAhead::setIdentifier(const std::string &identifier) {
	p_identifier = identifier;
}

void Ll::WriteAhead::createLog() {
	std::string file_name = p_path + "/" + p_identifier + ".wal";
	p_file->openSync(file_name, Linux::kFileCreate |
		Linux::kFileRead | Linux::kFileWrite);
}
void Ll::WriteAhead::loadLog() {
	std::string file_name = p_path + "/" + p_identifier + ".wal";
	p_file->openSync(file_name, Linux::kFileRead | Linux::kFileWrite);

	Linux::size_type position = 0;
	Linux::size_type length = p_file->lengthSync();
	while(position < length) {
		char head[20];
		p_file->readSync(20, head);

		uint32_t msg_length = OS::fromLe(*(uint32_t*)head);
		char *buffer = new char[msg_length];
		p_file->readSync(msg_length, buffer);
		
		char hash[16];
		MD5((uint8_t*)buffer, msg_length, (uint8_t*)hash);
		if(memcmp(head + 4, hash, 16) != 0)
			throw std::runtime_error("WriteAhead: Hash mismatch!");

		Db::Proto::WriteAhead message;
		if(!message.ParseFromArray(buffer, msg_length))
			throw std::runtime_error("Could not deserialize protobuf");
		
		std::cout << "wal: " << message.type() << std::endl;
		
		position += 20 + msg_length;
	}
}

void Ll::WriteAhead::submit(Db::Proto::WriteAhead &message,
		std::function<void(Error)> callback) {
	message.set_sequence(p_curSequence++);
	
	int msg_length = message.ByteSize();
	char *buffer = new char[msg_length + 20];

	*((uint32_t*)buffer) = OS::toLe(msg_length);
	
	if(!message.SerializeToArray(buffer + 20, msg_length))
		throw std::logic_error("Could not serialize protobuf");
	
	MD5((uint8_t*)(buffer + 20), msg_length, (uint8_t*)(buffer + 4));
	
	p_file->writeSync(msg_length + 20, buffer);
//	p_file->fdatasyncSync();
	callback(Error(true));
}

