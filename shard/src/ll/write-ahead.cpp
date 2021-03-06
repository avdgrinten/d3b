
#include <cstring>
#include <iostream>

#include "async.hpp"
#include "os/linux.hpp"

#include <Config.pb.h>

#include "ll/write-ahead.hpp"
#include "ll/crypto.hpp"

Ll::WriteAhead::WriteAhead() {
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
	p_file->seekEnd();
}

void Ll::WriteAhead::replay(Async::Callback<void(Db::Proto::LogEntry &)> on_entry) {
	p_file->seekTo(0);
	Linux::size_type position = 0;
	Linux::size_type length = p_file->lengthSync();
	while(position < length) {
		char head[20];
		p_file->readSync(20, head);

		uint32_t msg_length = OS::fromLe(*(uint32_t*)head);
		char *buffer = new char[msg_length];
		p_file->readSync(msg_length, buffer);
		
		char hash[16];
		Md5::hash(msg_length, buffer, hash);
		if(memcmp(head + 4, hash, 16) != 0)
			throw std::runtime_error("WriteAhead: Hash mismatch!");

		Db::Proto::LogEntry entry;
		if(!entry.ParseFromArray(buffer, msg_length))
			throw std::runtime_error("Could not deserialize protobuf");
		on_entry(entry);
		
		position += 20 + msg_length;
	}
}

void Ll::WriteAhead::log(Db::Proto::LogEntry &message,
		Async::Callback<void(Error)> callback) {
	int msg_length = message.ByteSize();
	char *buffer = new char[msg_length + 20];

	*((uint32_t*)buffer) = OS::toLe(msg_length);
	
	if(!message.SerializeToArray(buffer + 20, msg_length))
		throw std::logic_error("Could not serialize protobuf");
	
	Md5::hash(msg_length, buffer + 20, buffer + 4);
	
	p_file->writeSync(msg_length + 20, buffer);
//	p_file->fdatasyncSync();
	delete[] buffer;
	callback(Error(true));
}

