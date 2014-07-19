
#include <cstdint>
#include <cstring>
#include <array>

#include <iostream>

#include "Os/Linux.h"
#include "Async.h"
#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "proto/Api.pb.h"
#include "Api/Server.hpp"

namespace Api {

Server::Connection::Connection(Server *server,
		Linux::SockStream *sock_stream)
		: p_server(server), p_sockStream(sock_stream) {
	p_sockStream->onClose([] () {
		std::cout << "Connection closed" << std::endl;
	});
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

void Server::Connection::p_readMessage(std::function<void(Error)> callback) {
	Async::staticSeries(std::make_tuple(
		[this](std::function<void(Error)> msg_callback) {
			p_sockStream->read(sizeof(PacketHead), &p_rawHead,
				[=]() { msg_callback(Error(true)); });
		},
		[this](std::function<void(Error)> msg_callback) {
			p_curPacket.opcode = OS::fromLeU32(p_rawHead.opcode);
			p_curPacket.length = OS::fromLeU32(p_rawHead.length);
			p_curPacket.seqNumber = OS::fromLeU32(p_rawHead.seqNumber);
			
			p_curBuffer = new char[p_curPacket.length];
			p_sockStream->read(p_curPacket.length, p_curBuffer,
				[=]() { msg_callback(Error(true)); });
		},
		[this](std::function<void(Error)> msg_callback) {
			p_processMessage();
			msg_callback(Error(true));
		},
		[this](std::function<void(Error)> msg_callback) {
			delete[] p_curBuffer;
			p_curBuffer = nullptr;
			msg_callback(Error(true));
		}
	), callback);
}

void Server::Connection::p_processMessage() {
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
			return;
		}

		struct control_struct {
			Db::Proto::Query query;
		};
		control_struct *control = new control_struct;
		control->query = request.query();

		engine->query(&control->query, [=] (const Db::QueryData &rows) {
			Proto::SrRows response;
			for(int i = 0; i < rows.items.size(); i++)
				response.add_row_data(rows.items[i]);
			p_postResponse(Proto::kSrRows, seq_number, response);
		}, [=] (Error error) { /*FIXME: don't ignore error value */
			delete control;
			
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
			return;
		}
		
		struct control_struct {
			Db::trid_type trid;
			std::vector<Db::Mutation> mutations;
		};
		control_struct *control = new control_struct;
		for(int i = 0; i < request.updates_size(); i++) {
			const Proto::Update &update = request.updates(i);

			Db::Mutation mutation;
			if(update.action() == Proto::Actions::kActInsert) {
				mutation.type = Db::Mutation::kTypeInsert;
				mutation.storageIndex = engine->getStorage(update.storage_name());
				mutation.buffer = update.buffer();
			}else if(update.action() == Proto::Actions::kActUpdate) {
				mutation.type = Db::Mutation::kTypeModify;
				mutation.storageIndex = engine->getStorage(update.storage_name());
				mutation.documentId = update.id();
				mutation.buffer = update.buffer();
			}else throw std::runtime_error("Illegal update type");
			control->mutations.push_back(mutation);
		}
		Connection *self = this;

		Async::staticSeries(std::make_tuple(
			[=](std::function<void(Error)> tr_callback) {
				engine->transaction([=](Error error, Db::trid_type trid) {
					if(!error.ok())
						return tr_callback(error);
					control->trid = trid;
					tr_callback(Error(true));
				});
			},
			[=](std::function<void(Error)> tr_callback) {
				Async::eachSeries(control->mutations.begin(),
						control->mutations.end(), [=](Db::Mutation &mutation,
							std::function<void(Error)> each_callback) {
					engine->updateMutation(control->trid, &mutation, each_callback);
				}, tr_callback);
			},
			[=](std::function<void(Error)> tr_callback) {
				engine->submit(control->trid, tr_callback);
			},
			[=](std::function<void(Error)> tr_callback) {
				engine->commit(control->trid, tr_callback);
			}
		), [=](Error error) {
			Proto::SrFin fin_resp;
			p_postResponse(Proto::kSrFin, seq_number, fin_resp);
		});
	}else if(p_curPacket.opcode == Proto::kCqCreateStorage) {
		Proto::CqCreateStorage request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
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
			return;
		}
		
		int index = engine->getView(request.identifier());
		engine->unlinkView(index);
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqUploadExtern) {
		Proto::CqUploadExtern request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
			return;
		}
		
		auto fd = osIntf->createFile();
		fd->openSync(engine->getPath() + "/extern/" + request.file_name(),
				Linux::kFileCreate | Linux::kFileTrunc | Linux::kFileWrite);
		fd->pwriteSync(0, request.buffer().length(), request.buffer().c_str());
		fd->closeSync();
		
		Proto::SrFin response;
		p_postResponse(Proto::kSrFin, seq_number, response);
	}else if(p_curPacket.opcode == Proto::kCqDownloadExtern) {
		Proto::CqDownloadExtern request;
		if(!request.ParseFromArray(p_curBuffer, p_curPacket.length)) {
			Proto::SrFin response;
			response.set_success(false);
			response.set_err_code(Proto::kErrIllegalRequest);
			p_postResponse(Proto::kSrFin, seq_number, response);
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
		p_postResponse(Proto::kSrBlob, seq_number, blob_resp);
		delete[] buffer;
		
		Proto::SrFin fin_resp;
		p_postResponse(Proto::kSrFin, seq_number, fin_resp);
	}else{
		Proto::SrFin response;
		response.set_success(false);
		response.set_err_code(Proto::kErrIllegalRequest);
		p_postResponse(Proto::kSrFin, seq_number, response);
	}
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
		[connection](std::function<void(Error)> callback) {
			connection->p_readMessage(callback);
		} , [](Error error) {
			printf("fin\n");
		});
}

};

