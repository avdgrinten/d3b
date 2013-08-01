
#include <openssl/md5.h>

#include "Os/Linux.h"

#include "proto/Request.pb.h"

#include "Ll/WriteAhead.hpp"

Ll::WriteAhead::WriteAhead() {
	p_curSequence = 1;
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
}

void Ll::WriteAhead::submit(Db::Proto::WriteAhead &message,
		std::function<void()> callback) {
	message.set_sequence(p_curSequence++);
	
	int msg_length = message.ByteSize();
	char *buffer = new char(msg_length + 20);

	*((uint32_t*)buffer) = OS::toLe(msg_length);
	
	if(!message.SerializeToArray(buffer + 20, msg_length))
		throw std::logic_error("Could not serialize protobuf");
	
	MD5((uint8_t*)(buffer + 20), msg_length, (uint8_t*)(buffer + 4 + msg_length));
	
	p_file->writeSync(msg_length + 20, buffer);
	callback();
}

