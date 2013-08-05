
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

class OS {
public:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	static uint32_t toLeU32(uint32_t v) { return v; }
	static uint32_t toLeU64(uint32_t v) { return v; }
	static uint32_t fromLeU32(uint32_t v) { return v; }
	static uint64_t fromLeU64(uint64_t v) { return v; }
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	static uint32_t toLeU32(uint32_t v) {
		return ((v & 0x000000FF) << 24)
			| ((v & 0x0000FF00) <<  8)
			| ((v & 0x00FF0000) >>  8)
			| ((v & 0xFF000000) >> 24);
	}
	static uint32_t fromLeU32(uint32_t v) {
		return ((v & 0x000000FF) << 24)
			| ((v & 0x0000FF00) <<  8)
			| ((v & 0x00FF0000) >>  8)
			| ((v & 0xFF000000) >> 24);
	}
	static uint64_t toLeU64(uint64_t v) {
		return ((a & 0x00000000000000FFULL) << 56) | 
			((a & 0x000000000000FF00ULL) << 40) | 
			((a & 0x0000000000FF0000ULL) << 24) | 
			((a & 0x00000000FF000000ULL) <<  8) | 
			((a & 0x000000FF00000000ULL) >>  8) | 
			((a & 0x0000FF0000000000ULL) >> 24) | 
			((a & 0x00FF000000000000ULL) >> 40) | 
			((a & 0xFF00000000000000ULL) >> 56);
	}
	static uint64_t fromLeU64(uint64_t v) {
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
	
	static uint64_t toLe(uint64_t v) { return toLeU64(v); }
	static uint64_t fromLe(uint64_t v) { return fromLeU64(v); }
};

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
	
	struct RwInfo {
		size_type length;
		size_type index;
		char *buffer;
		std::function<void()> callback;
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
		void read(size_type length, void *buffer,
				std::function<void()> callback);
		void write(size_type length, const void *buffer,
				std::function<void()> callback);
		
		void onClose(std::function<void()> callback);
	private:
		SockStream(int socket_fd);

		int p_socketFd;
		
		std::function<void()> p_onClose;
		
		std::queue<RwInfo> p_readQueue;
		std::queue<RwInfo> p_writeQueue;
		std::function<void(int)> p_epollFunctor;
		bool p_epollInstalled;

		void p_epollUpdate();
		void p_epollProcess(int events);
	};

	class SockServer {
	public:
		void listen(int port);
		
		void onConnect(std::function<void(SockStream*)> callback);

	private:
		int p_socketFd;
		
		std::function<void(SockStream*)> p_onConnect;
	};

	class EventFd {
	public:
		EventFd();
		void onEvent(std::function<void()> on_event);
		void fire();
		void install();
		void uninstall();
		
	private:
		int p_eventFd;
		std::function<void()> p_epollFunctor;
	};
	
	bool fileExists(const std::string &path);
	void mkDir(const std::string &path);
	void rmDir(const std::string &path);
	
	std::unique_ptr<File> createFile();
	std::unique_ptr<SockServer> createSockServer();
	std::unique_ptr<EventFd> createEventFd();
	
	Linux();
	
	void nextTick(std::function<void()> callback);
	void processIO();

private:
	std::stack<std::function<void()>> p_tickStack;
	int p_epollFd;
};

extern Linux *osIntf;

