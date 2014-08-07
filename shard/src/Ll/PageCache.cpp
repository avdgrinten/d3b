
#include <cstring>
#include <cassert>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Ll/PageCache.hpp"

// --------------------------------------------------------
// PageInfo
// --------------------------------------------------------

PageInfo::PageInfo(PageCache *cache, PageNumber number)
	: p_cache(cache), p_number(number) { }

void PageInfo::diskRead() {
	std::lock_guard<std::mutex> lock(p_cache->p_mutex);

	p_cache->p_file->preadSync(p_number * p_cache->p_pageSize,
			p_cache->p_pageSize, p_buffer);
	
	p_isReady = true;
	for(auto it = callbacks.begin(); it != callbacks.end(); it++)
		(*it)();
	callbacks.clear();
}

// --------------------------------------------------------
// PageCache
// --------------------------------------------------------

PageCache::PageCache(int page_size, TaskPool *io_pool)
		: p_pageSize(page_size), p_usedCount(0), p_ioPool(io_pool) {
	p_file = osIntf->createFile();
}

void PageCache::open(const std::string &path) {
	p_file->openSync(path, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
}

void PageCache::initializePage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_presentPages.find(number);
	if(iterator == p_presentPages.end()) {
		PageInfo *info = new PageInfo(this, number);
		info->p_buffer = new char[p_pageSize];
		info->p_useCount = 1;
		info->p_isReady = true;
		info->p_isDirty = false;
		
		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;

		memset(info->p_buffer, 0, p_pageSize);
		lock.unlock();
		callback(info->p_buffer);
	}else{
		PageInfo *info = iterator->second;
		assert(info->p_useCount == 0);
		assert(info->p_isReady);
		info->p_useCount = 1;

		p_usedCount++;

		lock.unlock();
		callback(info->p_buffer);
	}
}
void PageCache::readPage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	if(iterator == p_presentPages.end()) {
		PageInfo *info = new PageInfo(this, number);
		info->p_buffer = new char[p_pageSize];
		info->p_useCount = 1;
		info->p_isReady = false;
		info->p_isDirty = false;

		auto *read_closure = new ReadClosure(this, number, callback);
		TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
		info->callbacks.push_back(wrapper);
		
		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;

		p_ioPool->submit(ASYNC_MEMBER(info, &PageInfo::diskRead));
	}else{
		PageInfo *info = iterator->second;
		if(info->p_useCount == 0)
			p_usedCount++;
		info->p_useCount++;
		
		if(info->p_isReady) {
			char *buffer = info->p_buffer;
			lock.unlock(); // NOTE: unlock before entering callbacks
			callback(buffer);
		}else{
			auto *closure = new ReadClosure(this, number, callback);
			TaskCallback wrapper(ASYNC_MEMBER(closure, &ReadClosure::complete));
			info->callbacks.push_back(wrapper);
		}
	}
}
void PageCache::writePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	PageInfo *info = p_presentPages.at(number);
	info->p_isDirty = true;
}
void PageCache::releasePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	assert(iterator != p_presentPages.end());

	PageInfo *info = iterator->second;
	assert(info->p_useCount > 0);
	info->p_useCount--;

	if(info->p_useCount == 0) {
		p_usedCount--;

		if(info->p_isDirty)
			p_file->pwriteSync(number * p_pageSize, p_pageSize, info->p_buffer);
		delete[] info->p_buffer;
		p_presentPages.erase(iterator);
	}
}

int PageCache::getPageSize() {
	return p_pageSize;
}

int PageCache::getUsedCount() {
	return p_usedCount;
}

// --------------------------------------------------------
// PageCache::ReadClosure
// --------------------------------------------------------

PageCache::ReadClosure::ReadClosure(PageCache *cache, PageNumber page_number,
		Async::Callback<void(char *)> callback)
	: p_cache(cache), p_pageNumber(page_number), p_callback(callback) { }

void PageCache::ReadClosure::complete() {
	std::unique_lock<std::mutex> lock(p_cache->p_mutex);
	
	PageInfo *info = p_cache->p_presentPages.at(p_pageNumber);
	char *buffer = info->p_buffer;
	lock.unlock(); // NOTE: unlock before entering callbacks
	p_callback(buffer);
	delete this;
}

