
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"

#include "Ll/PageCache.hpp"

PageCache::PageCache(int page_size) : p_pageSize(page_size) {
	p_file = osIntf->createFile();
}

void PageCache::open(const std::string &path) {
	p_file->openSync(path, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
}

char *PageCache::initializePage(PageNumber number) {
	PageInfo info;
	info.buffer = new char[p_pageSize];
	info.useCount = 1;
	info.isDirty = false;
	//std::cout << "Initialize " << number << std::endl;
	p_presentPages.insert(std::make_pair(number, info));
	return info.buffer;
}
void PageCache::readPage(PageNumber number,
		Async::Callback<void(char *)> callback) {
	auto find_it = p_presentPages.find(number);
	if(find_it == p_presentPages.end()) {
		PageInfo info;
		info.buffer = new char[p_pageSize];
		info.useCount = 1;
		info.isDirty = false;
		//std::cout << "Reading " << number << std::endl;
		p_file->preadSync(number * p_pageSize, p_pageSize, info.buffer);
		p_presentPages.insert(std::make_pair(number, info));
		callback(info.buffer);
	}else{
		PageInfo &info = find_it->second;
		info.useCount++;
		//std::cout << "Return " << number << std::endl;
		callback(info.buffer);
	}
}
void PageCache::writePage(PageNumber number) {
	
}
void PageCache::releasePage(PageNumber number) {

}

