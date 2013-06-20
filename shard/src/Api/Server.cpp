
#include <cstdint>
#include <cstring>
#include <array>

#include <iostream>

#include "Os/Linux.h"
#include "Async.h"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Transact.hpp"
#include "Db/Engine.hpp"

#include "proto/Api.pb.h"
#include "Api/Server.hpp"

namespace Api {

Server::Connection::Connection(Server *server,
		Linux::SockStream *sock_stream)
	: p_server(server), p_sockStream(sock_stream) {
}

void Server::Connection::p_postResponse(int opcode, int seq_number,
		const google::protobuf::MessageLite &response) {
	uint32_t length = response.ByteSize();
	char *buffer = new char[sizeof(PacketHead) + length];
	
	PacketHead *head = (PacketHead*)buffer;
	head->opcode = OS::toLeU32(opcode);
	head->length = OS::toLeU32(length);
	head->seqNumber = OS::toLeU32(seq_number);
	
	if(!response.SerializeToArray(buffer + sizeof(PacketHead), length))
		throw std::runtime_error("Could not serialize protobuf");
	
	p_sockStream->write(sizeof(PacketHead) + length, buffer, [buffer] () {
		delete[] buffer;
	});
}

void Server::Connection::p_readMessage(Async::Callback callback) {
	Async::staticSeries(std::make_tuple(
		[this](Async::Callback callback) {
			p_sockStream->read(sizeof(PacketHead), &p_rawHead, callback);
		},
		[this](Async::Callback callback) {
			p_curPacket.opcode = OS::fromLeU32(p_rawHead.opcode);
			p_curPacket.length = OS::fromLeU32(p_rawHead.length);
			p_curPacket.seqNumber = OS::fromLeU32(p_rawHead.seqNumber);
			
			p_curBuffer = new char[p_curPacket.length];
			p_sockStream->read(p_curPacket.length, p_curBuffer, callback);
		},
		[this](Async::Callback callback) {
			p_processMessage(callback);
		},
		[this](Async::Callback callback) {
			delete[] p_curBuffer;
			p_curBuffer = nullptr;
			callback();
		}
	), callback);
}

void Server::Connection::p_processMessage(Async::Callback callback) {
	uint32_t seq_number = p_curPacket.seqNumber;
	//std::cout << "Message: " << p_curPacket.opcode << std::endl;

	Db::Engine *engine = p_server->getEngine();
	if(p_curPacket.opcode == Proto::kCqQuery) {
		Proto::CqQuery request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}

		engine->query(request.query(),
				[this, seq_number] (const Db::Proto::Rows &rows) {
			Proto::SrRows response;
			for(int i = 0; i < rows.data_size(); i++)
				response.add_row_data(rows.data(i));
			p_postResponse(Proto::kSrRows, seq_number, response);
		}, [this, seq_number] () {
			Proto::SrFin response;
			p_postResponse(Proto::kSrFin, seq_number, response);
		});
	}else if(p_curPacket.opcode == Proto::kCqShortTransact) {
		Proto::CqShortTransact request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}
		
		struct control_struct {
			Db::trid_type trid;
		};
		control_struct *control = new control_struct;
		std::tuple<Error, Db::trid_type> result = engine->transaction
				([this, control, seq_number] (Db::TransactState state) {
			Db::Engine *engine = p_server->getEngine();
			if(state == Db::kStFixSuccess) {
				engine->commit(control->trid);
			}else if(state == Db::kStFixFailure) {
				std::cout << "FixFailure" << std::endl;
				engine->cleanup(control->trid);
			}else if(state == Db::kStCommit) {
				Proto::SrFin response;
				p_postResponse(Proto::kSrFin, seq_number, response);
				engine->cleanup(control->trid);
			}else if(state == Db::kStCleanup) {
				delete control;
			}else throw std::logic_error("Unexpected state update");
		});
		control->trid = std::get<1>(result);
		for(int i = 0; i < request.updates_size(); i++)
			engine->submit(control->trid, request.updates(i));
		engine->fix(control->trid);
	}else if(p_curPacket.opcode == Proto::kCqCreateStorage) {
		Proto::CqCreateStorage request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}

		engine->createStorage(request.config());
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqCreateView) {
		Proto::CqCreateView request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}

		engine->createView(request.config());
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUnlinkStorage) {
		Proto::CqUnlinkStorage request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}
		
		int index = engine->getStorage(request.identifier());
		engine->unlinkStorage(index);
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUnlinkView) {
		Proto::CqUnlinkView request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			callback();
			return;
		}
		
		int index = engine->getView(request.identifier());
		engine->unlinkView(index);
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else{
		Proto::SrFin response;
		response.set_success(false);
		response.set_err_code(Proto::kErrIllegalRequest);
		p_postResponse(Proto::kSrFin, seq_number, response);
	}
	callback();
}

Server::Server(Db::Engine *engine) :p_engine(engine) {
}

void Server::start() {
	p_sockServer = osIntf->createSockServer();
	
	p_sockServer->onConnect([this](Linux::SockStream *stream) {
		p_onConnect(stream);
	});
	p_sockServer->listen(7963);
}

void Server::p_onConnect(Linux::SockStream *stream) {
	Connection *connection = new Connection(this, stream);
	
	Async::whilst([] () { return true; },
		[connection](Async::Callback callback) {
			connection->p_readMessage(callback);
		} , []() {
			printf("fin\n");
		});
}

};

