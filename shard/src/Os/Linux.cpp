
#include "Os/Linux.h"

#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include <iostream>

enum epollevt_type {
	evt_none, evt_read = 1, evt_write = 2, evt_hup = 4
};

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

std::unique_ptr<Linux::File> Linux::createFile() {
	return std::unique_ptr<Linux::File>(new Linux::File());
}

/* ------------------------------------------------------------------- */

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

	auto functor_ptr = new std::function<void(int)>([this](int evt) {
		int stream_fd = accept(p_socketFd, NULL, NULL);
		if(stream_fd == -1)
			throw std::runtime_error("accept() failed");
		
		SockStream *stream = new SockStream(stream_fd);
		p_onConnect(stream);
	});

	epoll_event event;
	event.data.ptr = functor_ptr;
	event.events = EPOLLIN;
	if(epoll_ctl(osIntf->p_epollFd, EPOLL_CTL_ADD,
			p_socketFd, &event) == - 1)
		throw std::runtime_error("epoll_ctl() failed");
}

void Linux::SockServer::onConnect
		(std::function<void(SockStream*)> callback) {
	p_onConnect = callback;
}

std::unique_ptr<Linux::SockServer> Linux::createSockServer() {
	return std::unique_ptr<Linux::SockServer>(new Linux::SockServer());
}

/* ------------------------------------------------------------------- */

Linux::SockStream::SockStream(int socket_fd)
		: p_socketFd(socket_fd), p_epollInstalled(false) {
	int fflags = fcntl(p_socketFd, F_GETFL);
	if(fflags == -1)
		throw std::runtime_error("fcntl(F_GETFL) failed");
	if(fcntl(p_socketFd, F_SETFL, fflags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL) failed");
	
	p_epollFunctor = [this](int events) { p_epollProcess(events); };
	p_epollUpdate();
}

void Linux::SockStream::onClose(std::function<void()> callback) {
	p_onClose = callback;
}

void Linux::SockStream::read(size_type length, void *buffer,
		std::function<void()> callback) {
	if(length > 0) {
		RwInfo rwinfo;
		rwinfo.length = length;
		rwinfo.buffer = (char*)buffer;
		rwinfo.index = 0;
		rwinfo.callback = callback;
		p_readQueue.push(rwinfo);
		p_epollUpdate();
	}else callback();
}

void Linux::SockStream::write(size_type length, const void *buffer,
		std::function<void()> callback) {
	if(length > 0) {
		RwInfo rwinfo;
		rwinfo.length = length;
		rwinfo.buffer = (char*)buffer;
		rwinfo.index = 0;
		rwinfo.callback = callback;
		p_writeQueue.push(rwinfo);
		p_epollUpdate();
	}else callback();
}

void Linux::SockStream::p_epollUpdate() {
	epoll_event event;
	event.data.ptr = &p_epollFunctor;
	event.events = 0;
	if(!p_readQueue.empty())
		event.events |= EPOLLIN;
	if(!p_writeQueue.empty())
		event.events |= EPOLLOUT;
	event.events |= EPOLLRDHUP;
	
	if(p_epollInstalled && event.events == 0) {
		if(epoll_ctl(osIntf->p_epollFd, EPOLL_CTL_DEL,
				p_socketFd, &event) == - 1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_DEL) failed");
		p_epollInstalled = false;
	}else if(p_epollInstalled) {
		if(epoll_ctl(osIntf->p_epollFd, EPOLL_CTL_MOD,
				p_socketFd, &event) == - 1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_MOD) failed");
	}else{
		if(epoll_ctl(osIntf->p_epollFd, EPOLL_CTL_ADD,
				p_socketFd, &event) == - 1)
			throw std::runtime_error("epoll_ctl(EPOLL_CTL_ADD) failed");
		p_epollInstalled = true;
	}
}

void Linux::SockStream::p_epollProcess(int events) {
	if(events & evt_read) {
		if(p_readQueue.empty())
			throw std::logic_error("Read queue is empty");
		RwInfo &rwinfo = p_readQueue.front();

		ssize_t bytes = ::read(p_socketFd, rwinfo.buffer + rwinfo.index,
				rwinfo.length - rwinfo.index);
		if(bytes == -1)
			throw std::runtime_error("Read failed");
		
		rwinfo.index += bytes;
		if(rwinfo.index == rwinfo.length) {
			auto callback = rwinfo.callback;
			p_readQueue.pop();
			if(p_readQueue.empty())
				p_epollUpdate();
			callback();
		}
	}
	if(events & evt_write) {
		if(p_writeQueue.empty())
			throw std::logic_error("Write queue is empty");
		RwInfo &rwinfo = p_writeQueue.front();

		ssize_t bytes = ::write(p_socketFd, rwinfo.buffer + rwinfo.index,
				rwinfo.length - rwinfo.index);
		if(bytes == -1)
			throw std::runtime_error("Write failed");
		
		rwinfo.index += bytes;
		if(rwinfo.index == rwinfo.length) {
			auto callback = rwinfo.callback;
			p_writeQueue.pop();
			if(p_writeQueue.empty())
				p_epollUpdate();
			callback();
		}
	}
	
	if(events & evt_hup) {
		p_onClose();
		if(close(p_socketFd) != 0)
			throw std::runtime_error("close() failed");
	}
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

/* ------------------------------------------------------------------- */

Linux::Linux() {
	p_epollFd = epoll_create1(0);
	if(p_epollFd == -1)
		throw std::runtime_error("epoll_create() failed");
}

void Linux::processIO() {
	epoll_event events[16];
	int n = epoll_wait(p_epollFd, events, 16, 10);
	if(n == -1)
		throw std::runtime_error("epoll_wait() failed");

	for(int i = 0; i < n; i++) {
		int type = 0;
		if(events[i].events & EPOLLIN)
			type |= epollevt_type::evt_read;
		if(events[i].events & EPOLLOUT)
			type |= epollevt_type::evt_write;
		if(events[i].events & EPOLLHUP)
			type |= epollevt_type::evt_hup;
		if(events[i].events & EPOLLRDHUP)
			type |= epollevt_type::evt_hup;

		auto functor_ptr = (std::function<void(int)>*)
				events[i].data.ptr;
		(*functor_ptr)(type);
	}
}

