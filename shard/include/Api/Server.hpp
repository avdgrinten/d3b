
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
		
		void onClose();
		void onRead(int size, const char *buffer);
		void onWriteItem();

		void processMessage();
		
		Server *p_server;
		Linux::SockStream *p_sockStream;
		
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
	};
	
	void p_onConnect(Linux::SockStream *stream);
	
	Db::Engine *p_engine;
	std::unique_ptr<Linux::SockServer> p_sockServer;

	class QueryClosure {
	public:
		QueryClosure(Db::Engine *engine, Connection *connection,
				ResponseId response_id);
		
		void execute(size_t packet_size, const void *packet_buffer);

	private:
		void onData(Db::QueryData &data);
		void complete(Error error);

		Db::Engine *p_engine;
		Connection *p_connection;
		ResponseId p_responseId;

		Db::Query p_query;
	};

	class ShortTransactClosure {
	public:
		ShortTransactClosure(Db::Engine *engine, Connection *connection,
				ResponseId response_id);

		void execute(size_t packet_size, const void *packet_buffer);
	
	private:
		void onTransaction(Error, Db::TransactionId transaction_id);
		void updateMutation();
		void onUpdateMutation(Error error);
		void onSubmit(Error error);
		void onCommit(Db::SequenceId sequence_id);

		Db::Engine *p_engine;
		Connection *p_connection;
		ResponseId p_responseId;

		Db::TransactionId p_transactionId;
		std::vector<Db::Mutation> p_mutations;
		int p_updatedMutationsCount;
	};
};

};

