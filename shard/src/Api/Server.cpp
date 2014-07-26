
#include <cstdint>
#include <cstring>
#include <array>

#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "proto/Api.pb.h"
#include "Api/Server.hpp"

namespace Api {

Server::Connection::Connection(Server *server, Linux::SockStream *sock_stream)
		: p_server(server), p_sockStream(sock_stream),
			p_readOffset(0), p_validHead(false) {
	p_eventFd = osIntf->createEventFd();

	p_sockStream->setCloseCallback(ASYNC_MEMBER(this, &Connection::onClose));
	p_sockStream->setReadCallback(ASYNC_MEMBER(this, &Connection::onRead));
}

void Server::Connection::postResponse(int opcode, int seq_number,
		const google::protobuf::MessageLite &response) {
	uint32_t length = response.ByteSize();
	char *buffer = new char[sizeof(PacketHead) + length];
	
	PacketHead *head = (PacketHead*)buffer;
	head->opcode = OS::toLeU32(opcode);
	head->length = OS::toLeU32(length);
	head->seqNumber = OS::toLeU32(seq_number);
	
	if(!response.SerializeToArray(buffer + sizeof(PacketHead), length))
		throw std::runtime_error("Could not serialize protobuf");
	
	SendQueueItem item;
	item.length = sizeof(PacketHead) + length;
	item.buffer = buffer;
	p_sendQueue.push(item);
	p_eventFd->increment();
}

void Server::Connection::onRead(int size, const char *buffer) {
	int offset = 0;
	while(offset < size) {
		if(!p_validHead) {
			int copy = std::min(PacketHead::kStructSize - p_readOffset, size - offset);
			memcpy(p_headBuffer + p_readOffset, buffer + offset, copy);
			offset += copy;
			p_readOffset += copy;
			
			if(p_readOffset == PacketHead::kStructSize) {
				p_curPacket.opcode = OS::unpackLe32(p_headBuffer + PacketHead::kOpcode);
				p_curPacket.length = OS::unpackLe32(p_headBuffer + PacketHead::kLength);
				p_curPacket.seqNumber = OS::unpackLe32(p_headBuffer + PacketHead::kResponseId);
				p_bodyBuffer = new char[p_curPacket.length];
				p_readOffset = 0;
				p_validHead = true;
			}
		}else{
			//FIXME: ugly cast
			int copy = std::min((int)p_curPacket.length - p_readOffset, size - offset);
			memcpy(p_bodyBuffer + p_readOffset, buffer + offset, copy);
			offset += copy;
			p_readOffset += copy;

			if(p_readOffset == p_curPacket.length) {
				processMessage();
				delete[] p_bodyBuffer;
				p_readOffset = 0;
				p_validHead = false;
			}
		}
	}
}

void Server::Connection::processWrite() {
	if(!p_sendQueue.empty()) {
		SendQueueItem &item = p_sendQueue.front();
		p_sockStream->write(item.length, item.buffer,
				ASYNC_MEMBER(this, &Connection::onWriteItem));
	}else{
		p_eventFd->wait(ASYNC_MEMBER(this, &Connection::processWrite));
	}
}
void Server::Connection::onWriteItem() {
	SendQueueItem &item = p_sendQueue.front();
	delete[] item.buffer;
	p_sendQueue.pop();

	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &Connection::processWrite));
}

void Server::Connection::onClose() {
	std::cout << "Connection closed" << std::endl;
}

Server::QueryClosure::QueryClosure(Db::Engine *engine, Connection *connection,
		ResponseId response_id)
	: p_engine(engine), p_connection(connection), p_responseId(response_id) { }

void Server::QueryClosure::execute(size_t packet_size, const void *packet_buffer) {
	Proto::CqQuery request;
	if(!request.ParseFromArray(packet_buffer, packet_size)) {
		Proto::SrFin response;
		response.set_success(false);
		response.set_err_code(Proto::kErrIllegalRequest);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);
		return;
	}
	
	p_query.viewIndex = p_engine->getView(request.view_name());
	if(request.has_sequence_id()) {
		p_query.sequenceId = request.sequence_id();
	}else{
		p_query.sequenceId = p_engine->currentSequenceId();
	}
	if(request.has_from_key()) {
		p_query.useFromKey = true;
		p_query.fromKey = request.from_key();
	}
	if(request.has_to_key()) {
		p_query.useFromKey = true;
		p_query.fromKey = request.to_key();
	}
	if(request.has_limit())
		p_query.limit = request.limit();

	p_engine->query(&p_query, ASYNC_MEMBER(this, &QueryClosure::onData),
			ASYNC_MEMBER(this, &QueryClosure::complete));
}
void Server::QueryClosure::onData(Db::QueryData &rows) {
	Proto::SrRows response;
	for(int i = 0; i < rows.items.size(); i++)
		response.add_row_data(rows.items[i]);
	p_connection->postResponse(Proto::kSrRows, p_responseId, response);
}
void Server::QueryClosure::complete(Error error) {
	// FIXME: don't ignore error value
	Proto::SrFin response;
	p_connection->postResponse(Proto::kSrFin, p_responseId, response);

	delete this;
}

Server::ShortTransactClosure::ShortTransactClosure(Db::Engine *engine, Connection *connection,
		ResponseId response_id)
	: p_engine(engine), p_connection(connection), p_responseId(response_id),
	p_updatedMutationsCount(0) { }

void Server::ShortTransactClosure::execute(size_t packet_size, const void *packet_buffer) {
	Proto::CqShortTransact request;
	if(!request.ParseFromArray(packet_buffer, packet_size)) {
		Proto::SrFin response;
		response.set_success(false);
		response.set_err_code(Proto::kErrIllegalRequest);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);
		return;
	}

	for(int i = 0; i < request.updates_size(); i++) {
		const Proto::Update &update = request.updates(i);

		Db::Mutation mutation;
		if(update.action() == Proto::Actions::kActInsert) {
			mutation.type = Db::Mutation::kTypeInsert;
			mutation.storageIndex = p_engine->getStorage(update.storage_name());
			mutation.buffer = update.buffer();
		}else if(update.action() == Proto::Actions::kActUpdate) {
			mutation.type = Db::Mutation::kTypeModify;
			mutation.storageIndex = p_engine->getStorage(update.storage_name());
			mutation.documentId = update.id();
			mutation.buffer = update.buffer();
		}else throw std::runtime_error("Illegal update type");
		p_mutations.push_back(mutation);
	}

	p_engine->transaction(ASYNC_MEMBER(this, &ShortTransactClosure::onTransaction));
};
void Server::ShortTransactClosure::onTransaction(Error error,
		Db::TransactionId transaction_id) {
	p_transactionId = transaction_id;
	updateMutation();
}
void Server::ShortTransactClosure::updateMutation() {
	if(p_updatedMutationsCount == p_mutations.size()) {
		p_engine->submit(p_transactionId,
				ASYNC_MEMBER(this, &ShortTransactClosure::onSubmit));
	}else{
		p_engine->updateMutation(p_transactionId,
				p_mutations[p_updatedMutationsCount],
				ASYNC_MEMBER(this, &ShortTransactClosure::onUpdateMutation));
	}
}
void Server::ShortTransactClosure::onUpdateMutation(Error error) {
	p_updatedMutationsCount++;
	updateMutation();
}
void Server::ShortTransactClosure::onSubmit(Error error) {
	p_engine->commit(p_transactionId,
			ASYNC_MEMBER(this, &ShortTransactClosure::onCommit));
}
void Server::ShortTransactClosure::onCommit(Db::SequenceId sequence_id) {
	Proto::SrShortTransact response;
	response.set_sequence_id(sequence_id);
	p_connection->postResponse(Proto::kSrShortTransact, p_responseId, response);
	
	delete this;
}

void Server::Connection::processMessage() {
	uint32_t seq_number = p_curPacket.seqNumber;
	//std::cout << "Message: " << p_curPacket.opcode << std::endl;

	Db::Engine *engine = p_server->getEngine();
	if(p_curPacket.opcode == Proto::kCqQuery) {
		auto closure = new QueryClosure(engine, this, p_curPacket.seqNumber);
		closure->execute(p_curPacket.length, p_bodyBuffer);
	}else if(p_curPacket.opcode == Proto::kCqShortTransact) {
		auto closure = new ShortTransactClosure(engine, this, p_curPacket.seqNumber);
		closure->execute(p_curPacket.length, p_bodyBuffer);
	}else if(p_curPacket.opcode == Proto::kCqCreateStorage) {
		Proto::CqCreateStorage request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}

		engine->createStorage(request.config());
		
		Proto::SrFin response;
		postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqCreateView) {
		Proto::CqCreateView request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}

		engine->createView(request.config());
		
		Proto::SrFin response;
		postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUnlinkStorage) {
		Proto::CqUnlinkStorage request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}
		
		int index = engine->getStorage(request.identifier());
		engine->unlinkStorage(index);
		
		Proto::SrFin response;
		postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUnlinkView) {
		Proto::CqUnlinkView request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}
		
		int index = engine->getView(request.identifier());
		engine->unlinkView(index);
		
		Proto::SrFin response;
		postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUploadExtern) {
		Proto::CqUploadExtern request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}
		
		auto fd = osIntf->createFile();
		fd->openSync(engine->getPath() + "/extern/" + request.file_name(),
				Linux::kFileCreate | Linux::kFileTrunc | Linux::kFileWrite);
		fd->pwriteSync(0, request.buffer().length(), request.buffer().c_str());
		fd->closeSync();
		
		Proto::SrFin response;
		postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqDownloadExtern) {
		Proto::CqDownloadExtern request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			postResponse(Proto::kSrFin, seq_number, response);
			return;
		}
		
		auto fd = osIntf->createFile();
		fd->openSync(engine->getPath() + "/extern/" + request.file_name(),
				Linux::kFileRead);
		Linux::size_type length = fd->lengthSync();
		
		char *buffer = new char[length];
		fd->preadSync(0, length, buffer);
		fd->closeSync();
		
		Proto::SrBlob blob_resp;
		blob_resp.set_buffer(std::string(buffer, length));
		postResponse(Proto::kSrBlob, seq_number, blob_resp);
		delete[] buffer;
		
		Proto::SrFin fin_resp;
		postResponse(Proto::kSrFin, seq_number, fin_resp);
	}else{
		Proto::SrFin response;
		response.set_success(false);
		response.set_err_code(Proto::kErrIllegalRequest);
		postResponse(Proto::kSrFin, seq_number, response);
	}
}

Server::Server(Db::Engine *engine) : p_engine(engine) {
}

void Server::start() {
	p_sockServer = osIntf->createSockServer();
	
	p_sockServer->onConnect(ASYNC_MEMBER(this, &Server::p_onConnect));
	p_sockServer->listen(7963);
}

void Server::p_onConnect(Linux::SockStream *stream) {
	Connection *connection = new Connection(this, stream);
	connection->processWrite();
}


};

