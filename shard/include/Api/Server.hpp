
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
	class Connection {
	friend class Server;
	private:
		struct PacketHead {
			uint32_t opcode;
			uint32_t length;
			uint32_t seqNumber;
		};
		
		Connection(Server *server,
				Linux::SockStream *sock_stream);
		
		void p_postResponse(int opcode, int seq_number,
				const google::protobuf::MessageLite &reponse);
		void p_readMessage(Async::Callback callback);
		void p_processMessage(Async::Callback callback);
		
		Server *p_server;
		Linux::SockStream *p_sockStream;
		
		PacketHead p_rawHead;
		PacketHead p_curPacket;
		char *p_curBuffer;
	};
	
	void p_onConnect(Linux::SockStream *stream);
	
	Db::Engine *p_engine;
	std::unique_ptr<Linux::SockServer> p_sockServer;
};

};

