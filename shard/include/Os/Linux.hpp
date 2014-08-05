
#include <stdint.h>
#include <string>
#include <memory>
#include <stdexcept>

#include <queue>
#include <stack>

enum ErrorCode {
	kErrSuccess = 1,
	kErrUnknown = 2,

	kErrIllegalStorage = 128,
	kErrIllegalView = 129
};

class Error {
public:
	Error(bool success) {
		p_code = success ? kErrSuccess : kErrUnknown;
	}
	Error(ErrorCode code) : p_code(code) {
	}
	
	bool ok() {
		return p_code == kErrSuccess;
	}
private:
	ErrorCode p_code;
};

namespace OS {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline uint32_t toLeU32(uint32_t v) { return v; }
inline uint32_t fromLeU32(uint32_t v) { return v; }
inline uint64_t toLeU64(uint64_t v) { return v; }
inline uint64_t fromLeU64(uint64_t v) { return v; }
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline uint32_t toLeU32(uint32_t v) {
	return ((v & 0x000000FF) << 24)
		| ((v & 0x0000FF00) <<  8)
		| ((v & 0x00FF0000) >>  8)
		| ((v & 0xFF000000) >> 24);
}
inline uint32_t fromLeU32(uint32_t v) {
	return ((v & 0x000000FF) << 24)
		| ((v & 0x0000FF00) <<  8)
		| ((v & 0x00FF0000) >>  8)
		| ((v & 0xFF000000) >> 24);
}
inline uint64_t toLeU64(uint64_t v) {
	return ((a & 0x00000000000000FFULL) << 56) | 
		((a & 0x000000000000FF00ULL) << 40) | 
		((a & 0x0000000000FF0000ULL) << 24) | 
		((a & 0x00000000FF000000ULL) <<  8) | 
		((a & 0x000000FF00000000ULL) >>  8) | 
		((a & 0x0000FF0000000000ULL) >> 24) | 
		((a & 0x00FF000000000000ULL) >> 40) | 
		((a & 0xFF00000000000000ULL) >> 56);
}
inline uint64_t fromLeU64(uint64_t v) {
	return ((a & 0x00000000000000FFULL) << 56) | 
		((a & 0x000000000000FF00ULL) << 40) | 
		((a & 0x0000000000FF0000ULL) << 24) | 
		((a & 0x00000000FF000000ULL) <<  8) | 
		((a & 0x000000FF00000000ULL) >>  8) | 
		((a & 0x0000FF0000000000ULL) >> 24) | 
		((a & 0x00FF000000000000ULL) >> 40) | 
		((a & 0xFF00000000000000ULL) >> 56);
}
#else
#error "GCC byte order defines not available"
#endif

static void packLe32(void *pointer, uint32_t value) {
	*((uint32_t*)pointer) = toLeU32(value);
}
static uint32_t unpackLe32(void *pointer) {
	return fromLeU64(*((uint32_t*)pointer));
}
static void packLe64(void *pointer, uint64_t value) {
	*((uint64_t*)pointer) = toLeU64(value);
}
static uint64_t unpackLe64(void *pointer) {
	return fromLeU64(*((uint64_t*)pointer));
}

static uint64_t toLe(uint64_t v) { return toLeU64(v); }
static uint64_t fromLe(uint64_t v) { return fromLeU64(v); }

class LocalAsyncHost;

}; // namespace os

struct epoll_event;

class Linux {
public:
	typedef uint64_t off_type;
	typedef uint64_t size_type;

	enum FileMode {
		read = 1, write = 2,
		kFileRead = 1,
		kFileWrite = 2,
		kFileCreate = 4,
		kFileTrunc = 8
	};

	class EpollInterface {
	public:
		virtual void operator() (epoll_event &event) = 0;
	};
	
	class File {
	public:
		void openSync(const std::string &path, int mode);
		void readSync(const size_type size, void *buffer);
		void writeSync(const size_type size, const void *buffer);
		void pwriteSync(const off_type position, const size_type size, const void *buffer);
		void preadSync(const off_type position, const size_type size, void *buffer);
		void fsyncSync();
		void fdatasyncSync();
		void closeSync();
		size_type lengthSync();

	private:
		int p_fileFd;
	};
	
	class SockStream {
	friend class Linux;
	public:
		void setReadCallback(Async::Callback<void()> callback);
		void setWriteCallback(Async::Callback<void()> callback);
		void setCloseCallback(Async::Callback<void()> callback);

		bool isReadReady();
		bool isWriteReady();

		int tryRead(size_type length, void *buffer);
		int tryWrite(size_type length, const void *buffer);
	
	private:
		SockStream(int socket_fd);
		
		Async::Callback<void()> p_readCallback;
		Async::Callback<void()> p_writeCallback;
		Async::Callback<void()> p_closeCallback;
		
		int p_socketFd;
		bool p_readReady;
		bool p_writeReady;

		class EpollCallback : public EpollInterface {
		public:
			virtual void operator() (epoll_event &event);

			EpollCallback(SockStream *stream);

		private:
			SockStream *p_stream;
		};
		EpollCallback p_epollCallback;
	};

	class SockServer {
	public:
		SockServer();

		void listen(int port);
		
		void onConnect(Async::Callback<void(SockStream*)> callback);

	private:
		int p_socketFd;
		
		Async::Callback<void(SockStream*)> p_onConnect;

		class EpollCallback : public EpollInterface {
		public:
			virtual void operator() (epoll_event &event);

			EpollCallback(SockServer *server);

		private:
			SockServer *p_server;
		};
		EpollCallback p_epollCallback;
	};

	class EventFd {
	public:
		EventFd(OS::LocalAsyncHost *async_host);
		void increment();
		void wait(Async::Callback<void()> callback);
		
	private:
		OS::LocalAsyncHost *p_asyncHost;
		int p_eventFd;
		Async::Callback<void()> p_callback;

		class EpollCallback : public EpollInterface {
		public:
			virtual void operator() (epoll_event &event);

			EpollCallback(EventFd *event_fd);

		private:
			EventFd *p_eventFd;
		};
		EpollCallback p_epollCallback;
	};
	
	bool fileExists(const std::string &path);
	void mkDir(const std::string &path);
	void rmDir(const std::string &path);
	
	std::unique_ptr<File> createFile();
	std::unique_ptr<SockServer> createSockServer();
	std::unique_ptr<EventFd> createEventFd();
	std::unique_ptr<EventFd> createEventFd(OS::LocalAsyncHost *async_host);
};

namespace OS {

std::string readFileSync(const std::string &path);
void writeFileSync(const std::string &path, const std::string buffer);

class LocalAsyncHost {
friend class Linux::SockServer;
friend class Linux::SockStream;
friend class Linux::EventFd;
public:
	static void set(LocalAsyncHost *pointer);
	static LocalAsyncHost *get();

	LocalAsyncHost();
	LocalAsyncHost(const LocalAsyncHost &) = delete;
	LocalAsyncHost &operator= (const LocalAsyncHost &) = delete;

	void process();

private:
	int p_epollFd;
};

}; // namespace OS

extern Linux *osIntf;

