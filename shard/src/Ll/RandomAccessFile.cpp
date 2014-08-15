
#include <cstring>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Ll/RandomAccessFile.hpp"

namespace Ll {

RandomAccessFile::RandomAccessFile(const std::string &name,
		CacheHost *cache_host, TaskPool *io_pool)
	: p_pageCache(cache_host, 4096, io_pool), p_name(name) { }

void RandomAccessFile::setPath(const std::string &path) {
	p_path = path;
}

void RandomAccessFile::createFile() {
	p_pageCache.open(p_path + '/' + p_name + ".bin");
}

// --------------------------------------------------------
// RandomAccessFile::ReadClosure
// --------------------------------------------------------

RandomAccessFile::ReadClosure::ReadClosure(RandomAccessFile *file)
	: p_file(file) { }

void RandomAccessFile::ReadClosure::read(int64_t position, int64_t length,
		char *buffer, Async::Callback<void()> callback) {
	if(length == 0) {
		callback();
		return;
	}

	p_position = position;
	p_length = length;
	p_buffer = buffer;
	p_callback = callback;
	
	p_offset = 0;
	fetchPage();
}

void RandomAccessFile::ReadClosure::fetchPage() {
	int64_t file_offset = p_position + p_offset;
	int64_t page_number = file_offset / p_file->p_pageCache.getPageSize();

	p_file->p_pageCache.readPage(page_number,
			ASYNC_MEMBER(this, &ReadClosure::onPageLoad));
}
void RandomAccessFile::ReadClosure::onPageLoad(char *page_buffer) {
	int64_t file_offset = p_position + p_offset;
	int64_t page_number = file_offset / p_file->p_pageCache.getPageSize();
	int64_t page_offset = file_offset % p_file->p_pageCache.getPageSize();

	int64_t chunk_size = std::min(p_length - p_offset,
		p_file->p_pageCache.getPageSize() - page_offset);
	
	memcpy(p_buffer + p_offset, page_buffer + page_offset, chunk_size);
	p_file->p_pageCache.writePage(page_number);
	p_file->p_pageCache.releasePage(page_number);
	
	p_offset += chunk_size;
	if(p_offset == p_length) {
		p_callback();
	}else{
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ReadClosure::fetchPage));
	}
}

// --------------------------------------------------------
// RandomAccessFile::WriteClosure
// --------------------------------------------------------

RandomAccessFile::WriteClosure::WriteClosure(RandomAccessFile *file)
	: p_file(file) { }

void RandomAccessFile::WriteClosure::write(int64_t position, int64_t length,
		const char *buffer, Async::Callback<void()> callback) {
	if(length == 0) {
		callback();
		return;
	}

	p_position = position;
	p_length = length;
	p_buffer = buffer;
	p_callback = callback;
	
	p_offset = 0;
	fetchPage();
}

void RandomAccessFile::WriteClosure::fetchPage() {
	int64_t file_offset = p_position + p_offset;
	int64_t page_number = file_offset / p_file->p_pageCache.getPageSize();

	p_file->p_pageCache.readPage(page_number,
			ASYNC_MEMBER(this, &WriteClosure::onPageLoad));
}
void RandomAccessFile::WriteClosure::onPageLoad(char *page_buffer) {
	int64_t file_offset = p_position + p_offset;
	int64_t page_number = file_offset / p_file->p_pageCache.getPageSize();
	int64_t page_offset = file_offset % p_file->p_pageCache.getPageSize();

	int64_t chunk_size = std::min(p_length - p_offset,
		p_file->p_pageCache.getPageSize() - page_offset);
	
	memcpy(page_buffer + page_offset, p_buffer + p_offset, chunk_size);
	p_file->p_pageCache.writePage(page_number);
	p_file->p_pageCache.releasePage(page_number);
	
	p_offset += chunk_size;
	if(p_offset == p_length) {
		p_callback();
	}else{
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &WriteClosure::fetchPage));
	}
}

} // namespace Ll

