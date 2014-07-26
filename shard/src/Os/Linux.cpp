
#include "Async.hpp"
#include "Os/Linux.hpp"

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
		: p_socketFd(socket_fd), p_wantRead(false), p_wantWrite(false),
		p_epollInstalled(false), p_epollCallback(this) {
	int fflags = fcntl(p_socketFd, F_GETFL);
	if(fflags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if(fcntl(p_socketFd, F_SETFL, fflags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL) failed");
	
	p_epollUpdate();
}

void Linux::SockStream::onClose(Async::Callback<void()> callback) {
	p_onClose = callback;
}

void Linux::SockStream::read(size_type length, void *buffer,
		Async::Callback<void()> callback) {
	if(p_wantRead)
		throw std::logic_error("Concurrent read on SockStream");
	p_wantRead = true;
	p_readLength = length;
	p_readBuffer = (char *)buffer;
	p_readOffset = 0;
	p_readCallback = callback;
	p_epollUpdate();
}

void Linux::SockStream::write(size_type length, const void *buffer,
		Async::Callback<void()> callback) {
	if(p_wantWrite)
		throw std::logic_error("Concurrent write on SockStream");
	p_wantWrite = true;
	p_writeLength = length;
	p_writeBuffer = (const char *)buffer;
	p_writeOffset = 0;
	p_writeCallback = callback;
	p_epollUpdate();
}

void Linux::SockStream::p_epollUpdate() {
	epoll_event event;
	event.data.ptr = &p_epollCallback;
	event.events = 0;
	if(p_wantRead)
		event.events |= EPOLLIN;
	if(p_wantWrite)
		event.events |= EPOLLOUT;
	event.events |= EPOLLRDHUP;
	
	if(p_epollInstalled && event.events == 0) {
		if(epoll_ctl(OS::LocalAsyncHost::get()->p_epollFd, EPOLL_CTL_DEL,
				p_socketFd, &event) == -1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_DEL) failed");
		p_epollInstalled = false;
	}else if(p_epollInstalled) {
		if(epoll_ctl(OS::LocalAsyncHost::get()->p_epollFd, EPOLL_CTL_MOD,
				p_socketFd, &event) == -1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_MOD) failed");
	}else{
		if(epoll_ctl(OS::LocalAsyncHost::get()->p_epollFd, EPOLL_CTL_ADD,
				p_socketFd, &event) == -1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_ADD) failed");
		p_epollInstalled = true;
	}
}

void Linux::SockStream::p_epollProcess(epoll_event &event) {
	if(event.events & EPOLLIN) {
		if(!p_wantRead)
			throw std::logic_error("!p_wantRead");
		ssize_t bytes = ::read(p_socketFd, p_readBuffer + p_readOffset,
				p_readLength - p_readOffset);
		if(bytes == -1)
			throw std::runtime_error("Read failed");
		
		p_readOffset += bytes;
		if(p_readOffset == p_readLength) {
			p_wantRead = false;
			p_epollUpdate();
			p_readCallback();
		}
	}
	if(event.events & EPOLLOUT) {
		if(!p_wantWrite)
			throw std::logic_error("!p_wantWrite");
		ssize_t bytes = ::write(p_socketFd, p_writeBuffer + p_writeOffset,
				p_writeLength - p_writeOffset);
		if(bytes == -1)
			throw std::runtime_error("Write failed");
		
		p_writeOffset += bytes;
		if(p_writeOffset == p_writeLength) {
			p_wantWrite = false;
			p_epollUpdate();
			p_writeCallback();
		}
	}
	
	if(event.events & EPOLLHUP) {
		p_onClose();
		if(close(p_socketFd) != 0)
			throw std::runtime_error("close() failed");
	}
}

Linux::SockStream::EpollCallback::EpollCallback(SockStream *stream)
	: p_stream(stream) { }

void Linux::SockStream::EpollCallback::operator() (epoll_event &event) {
	p_stream->p_epollProcess(event);
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


// --------------------------------------------------------
// LocalAsyncHost
// --------------------------------------------------------

namespace OS {

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
	if(n == -1)
		throw std::runtime_error("epoll_wait() failed");

	for(int i = 0; i < n; i++) {
		auto interface = (Linux::EpollInterface *)events[i].data.ptr;
		(*interface)(events[i]);
	}
}

} // namespace OS

