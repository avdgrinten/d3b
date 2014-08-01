
#include "Ll/Tls.hpp"
#include "proto/Api.pb.h"

namespace Api {

class Server {
public:
	Server(Db::Engine *engine);
	
	void start();

	inline Db::Engine *getEngine() {
		return p_engine;
	}

private:
	typedef uint32_t ResponseId;

	class Connection {
	public:
		Connection(Server *server,
				Linux::SockStream *sock_stream);
		
		void processWrite();
		
		void postResponse(int opcode, int seq_number,
				const google::protobuf::MessageLite &reponse);

		void parseMutation(Db::Mutation &mutation, const Proto::Mutation &pb_mutation);
		void parseConstraint(Db::Constraint &constraint, const Proto::Constraint &pb_constraint);

	private:
		struct PacketHead {
			enum Fields {
				kOpcode = 0,
				kLength = 4,
				kResponseId = 8,
				// overall size of this struct
				kStructSize = 12
			};

			uint32_t opcode;
			uint32_t length;
			uint32_t seqNumber;
		};
		
		void onWriteItem();

		void onReadTls(int size, const char *buffer);
		void onWriteRaw(int size, const char *buffer);
		void onClose();

		void processMessage();
		
		Server *p_server;
		Linux::SockStream *p_sockStream;
		Ll::TlsServer::Channel p_tlsChannel;
		
		char p_headBuffer[PacketHead::kStructSize];
		PacketHead p_curPacket;
		char *p_bodyBuffer;
		int p_readOffset;
		bool p_validHead;

		struct SendQueueItem {
			size_t length;
			char *buffer;
		};

		std::queue<SendQueueItem> p_sendQueue;
		std::unique_ptr<Linux::EventFd> p_eventFd;
		std::mutex p_mutex;
	};
	
	void p_onConnect(Linux::SockStream *stream);
	
	Db::Engine *p_engine;
	std::unique_ptr<Linux::SockServer> p_sockServer;
	Ll::TlsServer p_tlsServer;

	class QueryClosure {
	public:
		QueryClosure(Db::Engine *engine, Connection *connection,
				ResponseId response_id);
		
		void execute(size_t packet_size, const void *packet_buffer);

	private:
		void onData(Db::QueryData &data);
		void complete(Db::QueryError error);

		Db::Engine *p_engine;
		Connection *p_connection;
		ResponseId p_responseId;

		Db::QueryRequest p_query;
	};

	class ShortTransactClosure {
	public:
		ShortTransactClosure(Db::Engine *engine, Connection *connection,
				ResponseId response_id);

		void execute(size_t packet_size, const void *packet_buffer);
	
	private:
		void onSubmit(Db::SubmitError);
		void onCommit(Db::SequenceId sequence_id);

		Db::Engine *p_engine;
		Connection *p_connection;
		ResponseId p_responseId;

		Db::TransactionId p_transactionId;
	};
	
	class ApplyClosure {
	public:
		ApplyClosure(Db::Engine *engine, Connection *connection,
				ResponseId response_id);
		
		void execute(size_t packet_size, const char *packet_buffer);

	private:
		void onSubmit(Db::SubmitError error);
		void onCommit(Db::SequenceId sequence_id);

		Db::Engine *p_engine;
		Connection *p_connection;
		ResponseId p_responseId;
	};
};

};

