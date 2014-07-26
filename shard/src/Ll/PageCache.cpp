
#include <cstring>
#include <cassert>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Ll/PageCache.hpp"

PageCache::PageCache(int page_size, TaskPool *io_pool)
		: p_pageSize(page_size), p_usedCount(0), p_ioPool(io_pool) {
	p_file = osIntf->createFile();
}

void PageCache::open(const std::string &path) {
	p_file->openSync(path, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
}

char *PageCache::initializePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_presentPages.find(number);
	if(iterator == p_presentPages.end()) {
		PageInfo info;
		info.buffer = new char[p_pageSize];
		info.useCount = 1;
		info.isReady = true;
		info.isDirty = false;
		
		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;

		memset(info.buffer, 0, p_pageSize);
		return info.buffer;
	}else{
		PageInfo &info = iterator->second;
		assert(info.isReady);
		if(info.useCount == 0)
			p_usedCount++;
		info.useCount++;
		return info.buffer;
	}
}
void PageCache::readPage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	if(iterator == p_presentPages.end()) {
		PageInfo info;
		info.buffer = new char[p_pageSize];
		info.useCount = 1;
		info.isReady = false;
		info.isDirty = false;

		auto *read_closure = new ReadClosure(this, number, callback);
		TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
		info.callbacks.push_back(wrapper);
		
		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;

		auto *submit_closure = new SubmitClosure(this, number);
		p_ioPool->submit(ASYNC_MEMBER(submit_closure, &SubmitClosure::process));
	}else{
		PageInfo &info = iterator->second;
		if(info.useCount == 0)
			p_usedCount++;
		info.useCount++;
		
		if(info.isReady) {
			char *buffer = info.buffer;
			lock.unlock(); // NOTE: unlock before entering callbacks
			callback(buffer);
		}else{
			auto *closure = new ReadClosure(this, number, callback);
			TaskCallback wrapper(ASYNC_MEMBER(closure, &ReadClosure::complete));
			info.callbacks.push_back(wrapper);
		}
	}
}
void PageCache::writePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	PageInfo &info = p_presentPages.at(number);
	info.isDirty = true;
}
void PageCache::releasePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	assert(iterator != p_presentPages.end());

	PageInfo &info = iterator->second;
	assert(info.useCount > 0);
	info.useCount--;

	if(info.useCount == 0) {
		p_usedCount--;

		if(info.isDirty)
			p_file->pwriteSync(number * p_pageSize, p_pageSize, info.buffer);
		delete[] info.buffer;
		p_presentPages.erase(iterator);
	}
}

int PageCache::getUsedCount() {
	return p_usedCount;
}

// --------------------------------------------------------
// PageCache::SubmitClosure
// --------------------------------------------------------

PageCache::SubmitClosure::SubmitClosure(PageCache *cache, PageNumber page_number)
	: p_cache(cache), p_pageNumber(page_number) { }

void PageCache::SubmitClosure::process() {
	std::unique_lock<std::mutex> lock(p_cache->p_mutex);

	PageInfo &info = p_cache->p_presentPages.at(p_pageNumber);
	p_cache->p_file->preadSync(p_pageNumber * p_cache->p_pageSize,
			p_cache->p_pageSize, info.buffer);
	
	info.isReady = true;
	for(auto it = info.callbacks.begin(); it != info.callbacks.end(); it++)
		(*it)();
	info.callbacks.clear();
	delete this;
}


// --------------------------------------------------------
// PageCache::ReadClosure
// --------------------------------------------------------

PageCache::ReadClosure::ReadClosure(PageCache *cache, PageNumber page_number,
		Async::Callback<void(char *)> callback)
	: p_cache(cache), p_pageNumber(page_number), p_callback(callback) { }

void PageCache::ReadClosure::complete() {
	std::unique_lock<std::mutex> lock(p_cache->p_mutex);
	
	PageInfo &info = p_cache->p_presentPages.at(p_pageNumber);
	char *buffer = info.buffer;
	lock.unlock(); // NOTE: unlock before entering callbacks
	p_callback(buffer);
	delete this;
}

