
#include <cstring>
#include <cassert>
#include <iostream>

#include "async.hpp"
#include "os/linux.hpp"
#include "ll/tasks.hpp"

#include "ll/page-cache.hpp"

// --------------------------------------------------------
// Cacheable
// --------------------------------------------------------

Cacheable::Cacheable() : p_moreRecentlyUsed(nullptr),
	p_lessRecentlyUsed(nullptr), p_alive(false) { }

// --------------------------------------------------------
// CacheHost
// --------------------------------------------------------

CacheHost::CacheHost() : p_activeFootprint(0), p_limit(0) {
	p_sentinel.p_moreRecentlyUsed = &p_sentinel;
	p_sentinel.p_lessRecentlyUsed = &p_sentinel;
}

void CacheHost::requestAcquire(Cacheable *item) {
	assert(item->getFootprint() < p_limit);

	item->acquire();
	
	std::unique_lock<std::mutex> lock(p_listMutex);
	
	assert(!item->p_alive);
	item->p_alive = true;
	p_activeFootprint += item->getFootprint();
	
	// insert the item at the end of the lru queue
	item->p_moreRecentlyUsed = &p_sentinel;
	item->p_lessRecentlyUsed = mostRecently();
	mostRecently()->p_moreRecentlyUsed = item;
	mostRecently() = item;

	while(p_activeFootprint > p_limit)
		releaseItem(leastRecently(), lock);
}
void CacheHost::onAccess(Cacheable *item) {
	std::lock_guard<std::mutex> lock(p_listMutex);
	
	if(!item->p_alive)
		return;
	
	// remove the item from the list
	Cacheable *less_recently = item->p_lessRecentlyUsed;
	Cacheable *more_recently = item->p_moreRecentlyUsed;
	less_recently->p_moreRecentlyUsed = more_recently;
	more_recently->p_lessRecentlyUsed = less_recently;
	
	// re-insert it at the end
	item->p_moreRecentlyUsed = &p_sentinel;
	item->p_lessRecentlyUsed = mostRecently();
	mostRecently()->p_moreRecentlyUsed = item;
	mostRecently() = item;
}
void CacheHost::afterRelease(Cacheable *item) {
	std::lock_guard<std::mutex> lock(p_listMutex);
}

void CacheHost::setLimit(int64_t limit) {
	p_limit = limit;
}

void CacheHost::releaseItem(Cacheable *item,
		std::unique_lock<std::mutex> &lock) {
	assert(lock.owns_lock());
	
	assert(item != &p_sentinel);
	item->p_alive = false;
	p_activeFootprint -= item->getFootprint();

	// remove the item from the list
	Cacheable *less_recently = item->p_lessRecentlyUsed;
	Cacheable *more_recently = item->p_moreRecentlyUsed;
	less_recently->p_moreRecentlyUsed = more_recently;
	more_recently->p_lessRecentlyUsed = less_recently;

	lock.unlock();
	item->release();
	lock.lock();
}

// NOTE: for the sentinel the meaning of p_lessRecentlyUsed
// and p_moreRecentlyUsed is swapped
Cacheable *&CacheHost::mostRecently() {
	return p_sentinel.p_lessRecentlyUsed;
}
Cacheable *&CacheHost::leastRecently() {
	return p_sentinel.p_moreRecentlyUsed;
}

// --------------------------------------------------------
// CacheHost::Sentinel
// --------------------------------------------------------

void CacheHost::Sentinel::acquire() {
	throw std::logic_error("Sentinel::acquire() called");
}
void CacheHost::Sentinel::release() {
	throw std::logic_error("Sentinel::release() called");
}
int64_t CacheHost::Sentinel::getFootprint() {
	throw std::logic_error("Sentinel::getFootprint() called");
}

// --------------------------------------------------------
// PageInfo
// --------------------------------------------------------

PageInfo::PageInfo(PageCache *cache, PageNumber number)
	: p_cache(cache), p_number(number), p_useCount(0), p_flags(0) { }

void PageInfo::acquire() {
	std::lock_guard<std::mutex> lock(p_cache->p_mutex);

	p_buffer = new char[p_cache->p_pageSize];
	memset(p_buffer, 0, p_cache->p_pageSize);

	if(p_flags & kFlagInitialize) {
		assert(p_useCount == 0);
		p_flags &= ~kFlagInitialize;
		p_flags |= kFlagLoaded;
		p_useCount = 1;

		assert(p_waitQueue.size() == 1);
		(p_waitQueue[0])();
		p_waitQueue.clear();
	}else{
		p_cache->p_ioPool->submit(ASYNC_MEMBER(this, &PageInfo::diskRead));
	}
}

void PageInfo::release() {
	std::unique_lock<std::mutex> lock(p_cache->p_mutex);
	
	p_flags |= kFlagRelease;
	if((p_flags & kFlagLoaded) && p_useCount == 0)
		doRelease(std::move(lock));
}

int64_t PageInfo::getFootprint() {
	return p_cache->p_pageSize;
}

void PageInfo::doRelease(std::unique_lock<std::mutex> lock) {
	// NOTE: pages are released when (1) they are loaded, (2) their release flag is set
	// and (3) their use count is zero
	assert((p_flags & kFlagLoaded) && p_flags & kFlagRelease);
	assert(p_useCount == 0);
	p_flags &= ~kFlagLoaded;
	p_flags &= ~kFlagRelease;

	if(p_flags & kFlagDirty) {
		p_cache->p_ioPool->submit(ASYNC_MEMBER(this, &PageInfo::diskWrite));
	}else{
		finishRelease(std::move(lock));
	}
}
void PageInfo::diskWrite() {
	p_cache->p_file->pwriteSync(p_number * p_cache->p_pageSize,
		p_cache->p_pageSize, p_buffer);
	p_flags &= ~kFlagDirty;

	std::unique_lock<std::mutex> lock(p_cache->p_mutex);
	finishRelease(std::move(lock));
}
void PageInfo::finishRelease(std::unique_lock<std::mutex> lock) {
	delete[] p_buffer;
	
	lock.unlock();
	p_cache->p_cacheHost->afterRelease(this);
	lock.lock();
	
	if(p_waitQueue.empty()) {
		auto iterator = p_cache->p_presentPages.find(p_number);
		assert(iterator != p_cache->p_presentPages.end());
		p_cache->p_presentPages.erase(iterator);
		delete this;
	}else{
		lock.unlock();
		p_cache->p_cacheHost->requestAcquire(this);
	}
}

void PageInfo::diskRead() {
	p_cache->p_file->preadSync(p_number * p_cache->p_pageSize,
			p_cache->p_pageSize, p_buffer);
	
	std::lock_guard<std::mutex> lock(p_cache->p_mutex);

	assert(p_useCount == 0);
	p_flags |= kFlagLoaded;
	p_useCount = p_waitQueue.size();

	for(auto it = p_waitQueue.begin(); it != p_waitQueue.end(); it++)
		(*it)();
	p_waitQueue.clear();
}

// --------------------------------------------------------
// PageCache
// --------------------------------------------------------

PageCache::PageCache(CacheHost *cache_host, int page_size, TaskPool *io_pool)
		: p_cacheHost(cache_host), p_pageSize(page_size), p_ioPool(io_pool) {
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
		p_presentPages.insert(std::make_pair(number, info));

		auto *read_closure = new ReadClosure(this, number, callback);
		TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
		info->p_waitQueue.push_back(wrapper);
		
		lock.unlock();
		p_cacheHost->requestAcquire(info);
	}else{
		PageInfo *info = iterator->second;
		
		assert(info->p_waitQueue.empty());
		assert(info->p_flags & PageInfo::kFlagLoaded);
		assert(info->p_useCount == 0);
		if(!(info->p_flags & PageInfo::kFlagRelease)) {
			info->p_useCount = 1;

			lock.unlock();
			callback(info->p_buffer);
		}else{
			info->p_flags |= PageInfo::kFlagInitialize;

			auto *read_closure = new ReadClosure(this, number, callback);
			TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
			info->p_waitQueue.push_back(wrapper);
		}
	}
}
void PageCache::readPage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	if(iterator == p_presentPages.end()) {
		PageInfo *info = new PageInfo(this, number);
		p_presentPages.insert(std::make_pair(number, info));

		auto *read_closure = new ReadClosure(this, number, callback);
		TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
		info->p_waitQueue.push_back(wrapper);
		
		lock.unlock();
		p_cacheHost->requestAcquire(info);
	}else{
		PageInfo *info = iterator->second;
		
		assert(!(info->p_flags & PageInfo::kFlagInitialize));
		if((info->p_flags & PageInfo::kFlagLoaded)
				&& !(info->p_flags & PageInfo::kFlagRelease)) {
			info->p_useCount++;

			lock.unlock();
			callback(info->p_buffer);
		}else{
			auto *read_closure = new ReadClosure(this, number, callback);
			TaskCallback wrapper(ASYNC_MEMBER(read_closure, &ReadClosure::complete));
			info->p_waitQueue.push_back(wrapper);
		}
	}
}
void PageCache::writePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	PageInfo *info = p_presentPages.at(number);
	info->p_flags |= PageInfo::kFlagDirty;
}
void PageCache::releasePage(PageNumber number) {
	std::unique_lock<std::mutex> lock(p_mutex);

	auto iterator = p_presentPages.find(number);
	assert(iterator != p_presentPages.end());

	PageInfo *info = iterator->second;
	assert(info->p_useCount > 0);
	info->p_useCount--;

	if(info->p_useCount == 0 && (info->p_flags & PageInfo::kFlagRelease))
		info->doRelease(std::move(lock));
}

int PageCache::getPageSize() {
	return p_pageSize;
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

