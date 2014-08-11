
#include "Async.hpp"
#include "Os/Linux.hpp"

#include <cassert>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <netinet/in.h>

#include <iostream>

Linux *osIntf = new Linux();

void Linux::File::openSync(const std::string &path, int mode) {
	int flags = 0;
	if(mode & kFileCreate)
		flags |= O_CREAT;
	if(mode & kFileTrunc)
		flags |= O_TRUNC;
	
	if((mode & FileMode::read) && (mode & FileMode::write)) {
		flags |= O_RDWR;
	}else if(mode & FileMode::read) {
		flags |= O_RDONLY;
	}else if(mode & FileMode::write) {
		flags |= O_WRONLY;
	}
	
	p_fileFd = open(path.c_str(), flags, 0644);
	if(p_fileFd == -1)
		throw std::runtime_error("Could not open file");
}

void Linux::File::closeSync() {
	if(close(p_fileFd) == -1)
		throw std::runtime_error("Could not close file");
}

void Linux::File::writeSync(Linux::size_type size, const void *buffer) {
	if(::write(p_fileFd, buffer, size) == -1)
		throw std::runtime_error("write() failed");
}
void Linux::File::readSync(Linux::size_type size, void *buffer) {
	if(::read(p_fileFd, buffer, size) == -1)
		throw std::runtime_error("write() failed");
}

void Linux::File::pwriteSync(Linux::off_type offset,
		Linux::size_type size, const void *buffer) {
	if(pwrite(p_fileFd, buffer, size, offset) == -1)
		throw std::runtime_error("pwrite() failed");
}

void Linux::File::preadSync(Linux::off_type offset,
		Linux::size_type size, void *buffer) {
	if(pread(p_fileFd, buffer, size, offset) == -1)
		throw std::runtime_error("pread() failed");
}

void Linux::File::seekTo(Linux::off_type position) {
	if(lseek(p_fileFd, position, SEEK_SET) == (off_t)(-1))
		throw std::runtime_error("lseek() failed");
}
void Linux::File::seekEnd() {
	if(lseek(p_fileFd, 0, SEEK_END) == (off_t)(-1))
		throw std::runtime_error("lseek() failed");
}

Linux::size_type Linux::File::lengthSync() {
	struct stat result;
	if(fstat(p_fileFd, &result) == -1)
		throw std::runtime_error("stat() failed");
	return result.st_size;
}

void Linux::File::fsyncSync() {
	if(::fsync(p_fileFd) == -1)
		throw std::runtime_error("fsync() failed");
}
void Linux::File::fdatasyncSync() {
	if(::fdatasync(p_fileFd) == -1)
		throw std::runtime_error("fsync() failed");
}

std::unique_ptr<Linux::File> Linux::createFile() {
	return std::unique_ptr<Linux::File>(new Linux::File());
}

std::unique_ptr<Linux::EventFd> Linux::createEventFd() {
	auto async_host = OS::LocalAsyncHost::get();
	return std::unique_ptr<Linux::EventFd>(new Linux::EventFd(async_host));
}
std::unique_ptr<Linux::EventFd> Linux::createEventFd(OS::LocalAsyncHost *async_host) {
	return std::unique_ptr<Linux::EventFd>(new Linux::EventFd(async_host));
}

/* ------------------------------------------------------------------- */

Linux::SockServer::SockServer() : p_epollCallback(this) { }

void Linux::SockServer::listen(int port) {
	p_socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if(p_socketFd == -1)
		throw std::runtime_error("socket() failed");

	int so_reuse_opt = 1;
	if(setsockopt(p_socketFd, SOL_SOCKET, SO_REUSEADDR,
			&so_reuse_opt, sizeof(int)) == -1)
		throw std::runtime_error("setsockopt() failed");

	sockaddr_in bind_addr;
	memset(&bind_addr, 0, sizeof(sockaddr_in));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(port);
	bind_addr.sin_addr.s_addr = 0;

	if(bind(p_socketFd, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) == -1)
		throw std::runtime_error("bind() failed");
	
	if(::listen(p_socketFd, 20) == -1)
		throw std::runtime_error("listen() failed");

	epoll_event event;
	event.data.ptr = &p_epollCallback;
	event.events = EPOLLIN;
	if(epoll_ctl(OS::LocalAsyncHost::get()->p_epollFd, EPOLL_CTL_ADD,
			p_socketFd, &event) == - 1)
		throw std::runtime_error("epoll_ctl() failed");
}

void Linux::SockServer::onConnect(Async::Callback<void(SockStream*)> callback) {
	p_onConnect = callback;
}

std::unique_ptr<Linux::SockServer> Linux::createSockServer() {
	return std::unique_ptr<Linux::SockServer>(new Linux::SockServer());
}

Linux::SockServer::EpollCallback::EpollCallback(SockServer *server)
	: p_server(server) { }

void Linux::SockServer::EpollCallback::operator() (epoll_event &event) {
	int stream_fd = accept(p_server->p_socketFd, NULL, NULL);
	if(stream_fd == -1)
		throw std::runtime_error("accept() failed");
	
	SockStream *stream = new SockStream(stream_fd);
	p_server->p_onConnect(stream);
}

/* ------------------------------------------------------------------- */

Linux::SockStream::SockStream(int socket_fd)
		: p_socketFd(socket_fd), p_readReady(false), p_writeReady(false),
		p_epollCallback(this) {
	int fflags = fcntl(p_socketFd, F_GETFL);
	if(fflags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if(fcntl(p_socketFd, F_SETFL, fflags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL) failed");

	epoll_event event;
	event.data.ptr = &p_epollCallback;
	event.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
	if(epoll_ctl(OS::LocalAsyncHost::get()->p_epollFd, EPOLL_CTL_ADD,
			p_socketFd, &event) == -1)
		throw std::runtime_error("epoll_ctl(EPOLL_CTL_ADD) failed");
}

void Linux::SockStream::setReadCallback(Async::Callback<void()> callback) {
	p_readCallback = callback;
}
void Linux::SockStream::setWriteCallback(Async::Callback<void()> callback) {
	p_writeCallback = callback;
}
void Linux::SockStream::setCloseCallback(Async::Callback<void()> callback) {
	p_closeCallback = callback;
}

bool Linux::SockStream::isReadReady() {
	return p_readReady;
}
bool Linux::SockStream::isWriteReady() {
	return p_writeReady;
}

int Linux::SockStream::tryRead(size_type length, void *buffer) {
	assert(p_readReady);
	ssize_t bytes = ::read(p_socketFd, buffer, length);
	if(bytes == -1) {
		if(errno == EAGAIN) {
			p_readReady = false;
			return 0;
		}else{
			throw std::runtime_error("Write failed");
		}
	}
	return bytes;
}

int Linux::SockStream::tryWrite(size_type length, const void *buffer) {
	assert(p_writeReady);
	ssize_t bytes = ::write(p_socketFd, buffer, length);
	if(bytes == -1) {
		if(errno == EAGAIN) {
			p_writeReady = false;
			return 0;
		}else{
			throw std::runtime_error("Write failed");
		}
	}
	return bytes;
}

Linux::SockStream::EpollCallback::EpollCallback(SockStream *stream)
	: p_stream(stream) { }

void Linux::SockStream::EpollCallback::operator() (epoll_event &event) {
	if(event.events & EPOLLIN) {
		if(!p_stream->p_readReady) {
			p_stream->p_readReady = true;
			p_stream->p_readCallback();
		}
	}

	if(event.events & EPOLLOUT) {
		if(!p_stream->p_writeReady) {
			p_stream->p_writeReady = true;
			p_stream->p_writeCallback();
		}
	}
	
	if(event.events & EPOLLHUP) {
		p_stream->p_closeCallback();
		if(close(p_stream->p_socketFd) != 0)
			throw std::runtime_error("close() failed");
	}
}

/* ------------------------------------------------------------------- */
Linux::EventFd::EventFd(OS::LocalAsyncHost *async_host) :
		p_asyncHost(async_host), p_epollCallback(this) {
	p_eventFd = eventfd(0, 0);
	if(p_eventFd == -1)
		throw std::runtime_error("eventfd() failed");
}

void Linux::EventFd::increment() {
	uint64_t value = 1;
	if(::write(p_eventFd, &value, 8) != 8)
		throw std::runtime_error("Could not write eventfd");
}

void Linux::EventFd::wait(Async::Callback<void()> callback) {
	p_callback = callback;

	epoll_event install_event;
	install_event.data.ptr = &p_epollCallback;
	install_event.events = EPOLLIN;
	if(epoll_ctl(p_asyncHost->p_epollFd, EPOLL_CTL_ADD, p_eventFd, &install_event) == - 1)
		throw std::runtime_error("epoll_ctl() failed");
}

Linux::EventFd::EpollCallback::EpollCallback(EventFd *event_fd)
	: p_eventFd(event_fd) { }

void Linux::EventFd::EpollCallback::operator() (epoll_event &event) {
	epoll_event uninstall_event;
	uninstall_event.events = EPOLLIN;
	if(epoll_ctl(p_eventFd->p_asyncHost->p_epollFd, EPOLL_CTL_DEL,
			p_eventFd->p_eventFd, &uninstall_event) == - 1)
		throw std::runtime_error("epoll_ctl() failed");

	uint64_t value;
	if(::read(p_eventFd->p_eventFd, &value, 8) != 8)
		throw std::runtime_error("Could not read eventfd");
	p_eventFd->p_callback();
}

/* ------------------------------------------------------------------- */

bool Linux::fileExists(const std::string &path) {
	if(access(path.c_str(), F_OK) == 0)
		return true;
	if(errno == ENOENT)
		return false;
	throw std::runtime_error("access() failed");
}
void Linux::mkDir(const std::string &path) {
	if(mkdir(path.c_str(), 0700) == -1)
		throw std::runtime_error("mkdir() failed");
}

namespace OS {

std::string readFileSync(const std::string &path) {
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(path, Linux::FileMode::read);
	Linux::size_type length = file->lengthSync();
	char *buffer = new char[length];
	file->preadSync(0, length, buffer);
	file->closeSync();
	return std::string(buffer, length);
}
void writeFileSync(const std::string &path, const std::string buffer) {
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(path, Linux::kFileWrite | Linux::kFileCreate | Linux::kFileTrunc);
	file->pwriteSync(0, buffer.size(), buffer.data());
	file->closeSync();
}

// --------------------------------------------------------
// LocalAsyncHost
// --------------------------------------------------------


__thread LocalAsyncHost *localHostPointer = nullptr;

void LocalAsyncHost::set(LocalAsyncHost *pointer) {
	localHostPointer = pointer;
}
LocalAsyncHost *LocalAsyncHost::get() {
	if(localHostPointer == nullptr)
		throw std::logic_error("No LocalAsyncHost defined");
	return localHostPointer;
}

LocalAsyncHost::LocalAsyncHost() {
	p_epollFd = epoll_create1(0);
	if(p_epollFd == -1)
		throw std::runtime_error("epoll_create() failed");
}

void LocalAsyncHost::process() {
	epoll_event events[16];
	int n = epoll_wait(p_epollFd, events, 16, -1);
	if(n == -1) {
		if(errno == EINTR)
			return;
		throw std::runtime_error("epoll_wait() failed");
	}

	for(int i = 0; i < n; i++) {
		auto interface = (Linux::EpollInterface *)events[i].data.ptr;
		(*interface)(events[i]);
	}
}

} // namespace OS

