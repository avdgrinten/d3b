
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
		: p_server(server), p_sockStream(sock_stream), p_tlsChannel(&server->p_tlsServer),
			p_readOffset(0), p_validHead(false) {
	p_eventFd = osIntf->createEventFd();

	p_sockStream->setCloseCallback(ASYNC_MEMBER(this, &Connection::onClose));
	p_sockStream->setReadCallback(ASYNC_MEMBER(&p_tlsChannel, &Ll::TlsServer::Channel::readRaw));

	p_tlsChannel.setWriteRawCallback(ASYNC_MEMBER(this, &Connection::onWriteRaw));
	p_tlsChannel.setReadTlsCallback(ASYNC_MEMBER(this, &Connection::onReadTls));
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
	
	std::lock_guard<std::mutex> lock(p_mutex);
	p_tlsChannel.writeTls(sizeof(PacketHead) + length, buffer);
	delete[] buffer;
}

void Server::Connection::onReadTls(int size, const char *buffer) {
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

				p_readOffset = 0;
				if(p_curPacket.length == 0) {
					processMessage();
				}else{
					p_bodyBuffer = new char[p_curPacket.length];
					p_validHead = true;
				}
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
void Server::Connection::onWriteRaw(int size, const char *input_buffer) {
	char *queued_buffer = new char[size];
	memcpy(queued_buffer, input_buffer, size);

	SendQueueItem item;
	item.length = size;
	item.buffer = queued_buffer;
	p_sendQueue.push(item);
	p_eventFd->increment();
}


void Server::Connection::processWrite() {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	if(!p_sendQueue.empty()) {
		SendQueueItem &item = p_sendQueue.front();

		lock.unlock();
		p_sockStream->write(item.length, item.buffer,
				ASYNC_MEMBER(this, &Connection::onWriteItem));
	}else{
		lock.unlock();
		p_eventFd->wait(ASYNC_MEMBER(this, &Connection::processWrite));
	}
}
void Server::Connection::onWriteItem() {
	std::unique_lock<std::mutex> lock(p_mutex);

	SendQueueItem &item = p_sendQueue.front();
	delete[] item.buffer;
	p_sendQueue.pop();
	
	lock.unlock();
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &Connection::processWrite));
}

void Server::Connection::onClose() {
	std::cout << "Connection closed" << std::endl;
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
	}else if(p_curPacket.opcode == Proto::kCqTransaction) {
		Proto::CqTransaction request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_error(Proto::kCodeParseError);
			postResponse(Proto::kSrFin, p_curPacket.seqNumber, response);
			return;
		}

		Db::TransactionId transaction_id = engine->transaction();

		Proto::SrFin response;
		response.set_error(Proto::kCodeSuccess);
		response.set_transaction_id(transaction_id);
		postResponse(Proto::kSrFin, p_curPacket.seqNumber, response);
	}else if(p_curPacket.opcode == Proto::kCqUpdate) {
		Proto::CqUpdate request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_error(Proto::kCodeParseError);
			postResponse(Proto::kSrFin, p_curPacket.seqNumber, response);
			return;
		}
	
		for(int i = 0; i < request.mutations_size(); i++) {
			const Proto::Mutation &pb_mutation = request.mutations(i);

			Db::Mutation mutation;
			parseMutation(mutation, pb_mutation);
			engine->updateMutation(request.transaction_id(), mutation);
		}
		
		for(int i = 0; i < request.constraints_size(); i++) {
			const Proto::Constraint &pb_constraint = request.constraints(i);

			Db::Constraint constraint;
			parseConstraint(constraint, pb_constraint);
			engine->updateConstraint(request.transaction_id(), constraint);
		}

		Proto::SrFin response;
		response.set_error(Proto::kCodeSuccess);
		postResponse(Proto::kSrFin, p_curPacket.seqNumber, response);
	}else if(p_curPacket.opcode == Proto::kCqApply) {
		auto closure = new ApplyClosure(engine, this, p_curPacket.seqNumber);
		closure->execute(p_curPacket.length, p_bodyBuffer);
	}else if(p_curPacket.opcode == Proto::kCqCreateStorage) {
		Proto::CqCreateStorage request;
		if(!request.ParseFromArray(p_bodyBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_error(Proto::kCodeParseError);
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
			response.set_error(Proto::kCodeParseError);
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
			response.set_error(Proto::kCodeParseError);
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
			response.set_error(Proto::kCodeParseError);
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
			response.set_error(Proto::kCodeParseError);
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
			response.set_error(Proto::kCodeParseError);
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
		response.set_error(Proto::kCodeIllegalRequest);
		postResponse(Proto::kSrFin, seq_number, response);
	}
}

void Server::Connection::parseMutation(Db::Mutation &mutation, const Proto::Mutation &pb_mutation) {
	auto engine = p_server->getEngine();
	if(pb_mutation.type() == Proto::Mutation::kTypeInsert) {
		mutation.type = Db::Mutation::kTypeInsert;
		mutation.storageIndex = engine->getStorage(pb_mutation.storage_name());
		mutation.buffer = pb_mutation.buffer();
	}else if(pb_mutation.type() == Proto::Mutation::kTypeModify) {
		mutation.type = Db::Mutation::kTypeModify;
		mutation.storageIndex = engine->getStorage(pb_mutation.storage_name());
		mutation.documentId = pb_mutation.document_id();
		mutation.buffer = pb_mutation.buffer();
	}else throw std::runtime_error("Illegal mutation type");
}

void Server::Connection::parseConstraint(Db::Constraint &constraint, const Proto::Constraint &pb_constraint) {
	auto engine = p_server->getEngine();
	if(pb_constraint.type() == Proto::Constraint::kTypeDocumentState) {
		constraint.type = Db::Constraint::kTypeDocumentState;
		constraint.storageIndex = engine->getStorage(pb_constraint.storage_name());
		constraint.documentId = pb_constraint.document_id();
		constraint.sequenceId = pb_constraint.sequence_id();
		constraint.mustExist = pb_constraint.must_exist();
		constraint.matchSequenceId = pb_constraint.match_sequence_id();
	}else throw std::runtime_error("Illegal constraint type");
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

// --------------------------------------------------------
// QueryClosure
// --------------------------------------------------------

Server::QueryClosure::QueryClosure(Db::Engine *engine, Connection *connection,
		ResponseId response_id)
	: p_engine(engine), p_connection(connection), p_responseId(response_id) { }

void Server::QueryClosure::execute(size_t packet_size, const void *packet_buffer) {
	Proto::CqQuery request;
	if(!request.ParseFromArray(packet_buffer, packet_size)) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeParseError);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);
		
		delete this;
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
void Server::QueryClosure::complete(Db::QueryError error) {
	if(error == Db::kQuerySuccess) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSuccess);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);
	}else throw std::logic_error("Unexpected error during query");

	delete this;
}

// --------------------------------------------------------
// ShortTransactClosure
// --------------------------------------------------------

Server::ShortTransactClosure::ShortTransactClosure(Db::Engine *engine, Connection *connection,
		ResponseId response_id)
	: p_engine(engine), p_connection(connection), p_responseId(response_id) { }

void Server::ShortTransactClosure::execute(size_t packet_size, const void *packet_buffer) {
	Proto::CqShortTransact request;
	if(!request.ParseFromArray(packet_buffer, packet_size)) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeParseError);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
		return;
	}

	p_transactionId = p_engine->transaction();

	for(int i = 0; i < request.mutations_size(); i++) {
		const Proto::Mutation &pb_mutation = request.mutations(i);

		Db::Mutation mutation;
		p_connection->parseMutation(mutation, pb_mutation);
		p_engine->updateMutation(p_transactionId, mutation);
	}
	
	for(int i = 0; i < request.constraints_size(); i++) {
		const Proto::Constraint &pb_constraint = request.constraints(i);

		Db::Constraint constraint;
		p_connection->parseConstraint(constraint, pb_constraint);
		p_engine->updateConstraint(p_transactionId, constraint);
	}

	p_engine->submit(p_transactionId,
			ASYNC_MEMBER(this, &ShortTransactClosure::onSubmit));
};
void Server::ShortTransactClosure::onSubmit(Db::SubmitError result) {
	if(result == Db::kSubmitSuccess) {
		p_engine->commit(p_transactionId,
				ASYNC_MEMBER(this, &ShortTransactClosure::onCommit));
	}else if(result == Db::kSubmitConstraintViolation) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSubmitConstraintViolation);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
	}else throw std::logic_error("Unexpected error during submit");
}
void Server::ShortTransactClosure::onCommit(Db::SequenceId sequence_id) {
	Proto::SrFin response;
	response.set_sequence_id(sequence_id);
	response.set_error(Proto::kCodeSuccess);
	p_connection->postResponse(Proto::kSrFin, p_responseId, response);
	
	delete this;
}

// --------------------------------------------------------
// ApplyClosure
// --------------------------------------------------------

Server::ApplyClosure::ApplyClosure(Db::Engine *engine, Connection *connection,
		ResponseId response_id)
	: p_engine(engine), p_connection(connection), p_responseId(response_id) { }

void Server::ApplyClosure::execute(size_t packet_size, const char *packet_buffer) {
	Proto::CqApply request;
	if(!request.ParseFromArray(packet_buffer, packet_size)) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeParseError);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
		return;
	}

	if(request.do_submit()) {
		p_engine->submit(request.transaction_id(),
				ASYNC_MEMBER(this, &ApplyClosure::onSubmit));
	}else if(request.do_commit()) {
		p_engine->commit(request.transaction_id(),
				ASYNC_MEMBER(this, &ApplyClosure::onCommit));
	}else{
		Proto::SrFin response;
		response.set_error(Proto::kCodeIllegalState);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
		return;
	}
}

void Server::ApplyClosure::onSubmit(Db::SubmitError result) {
	if(result == Db::kSubmitSuccess) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSuccess);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
	}else if(result == Db::kSubmitConstraintViolation) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSubmitConstraintViolation);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
	}else if(result == Db::kSubmitConstraintConflict) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSubmitConstraintConflict);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
	}else if(result == Db::kSubmitMutationConflict) {
		Proto::SrFin response;
		response.set_error(Proto::kCodeSubmitMutationConflict);
		p_connection->postResponse(Proto::kSrFin, p_responseId, response);

		delete this;
	}else throw std::logic_error("Unexpected error during submit");
}

void Server::ApplyClosure::onCommit(Db::SequenceId sequence_id) {
	Proto::SrFin response;
	response.set_error(Proto::kCodeSuccess);
	response.set_sequence_id(sequence_id);
	p_connection->postResponse(Proto::kSrFin, p_responseId, response);
	
	delete this;
}

} // namespace Api

