
#include <cstring>
#include <cassert>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"

#include "Ll/PageCache.hpp"

PageCache::PageCache(int page_size) : p_pageSize(page_size),
		p_usedCount(0) {
	p_file = osIntf->createFile();
}

void PageCache::open(const std::string &path) {
	p_file->openSync(path, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
}

char *PageCache::initializePage(PageNumber number) {
	auto find_it = p_presentPages.find(number);
	if(find_it == p_presentPages.end()) {
		PageInfo info;
		info.buffer = new char[p_pageSize];
		info.useCount = 1;
		info.isDirty = false;
		
		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;

		memset(info.buffer, 0, p_pageSize);
		return info.buffer;
	}else{
		PageInfo &info = find_it->second;
		if(info.useCount == 0)
			p_usedCount++;
		info.useCount++;
		return info.buffer;
	}
}
void PageCache::readPage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	auto find_it = p_presentPages.find(number);
	if(find_it == p_presentPages.end()) {
		PageInfo info;
		info.buffer = new char[p_pageSize];
		info.useCount = 1;
		info.isDirty = false;

		p_presentPages.insert(std::make_pair(number, info));
		p_usedCount++;
		
		p_file->preadSync(number * p_pageSize, p_pageSize, info.buffer);
		callback(info.buffer);
	}else{
		PageInfo &info = find_it->second;
		if(info.useCount == 0)
			p_usedCount++;
		info.useCount++;
		callback(info.buffer);
	}
}
void PageCache::writePage(PageNumber number) {
	
}
void PageCache::releasePage(PageNumber number) {
	auto iterator = p_presentPages.find(number);
	assert(iterator != p_presentPages.end());

	PageInfo &info = iterator->second;
	assert(info.useCount > 0);
	info.useCount--;

	if(info.useCount == 0) {
		p_usedCount--;

		p_file->pwriteSync(number * p_pageSize, p_pageSize, info.buffer);
		delete[] info.buffer;
		p_presentPages.erase(iterator);
	}
}

int PageCache::getUsedCount() {
	return p_usedCount;
}

