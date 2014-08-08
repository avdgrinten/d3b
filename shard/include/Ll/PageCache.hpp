
#ifndef D3B_LL_PAGE_CACHE_HPP
#define D3B_LL_PAGE_CACHE_HPP

#include <unordered_map>
#include <mutex>

class CacheHost;

class Cacheable {
friend class CacheHost;
public:
	Cacheable();

private:
	Cacheable *p_moreRecentlyUsed;
	Cacheable *p_lessRecentlyUsed;
};

class CacheHost {
public:
	CacheHost();
	
	void pushItem(Cacheable *item);
	void removeItem(Cacheable *item);

private:
	Cacheable *p_mostRecentlyUsed;
	Cacheable *p_leastRecentlyUsed;

	std::mutex p_listMutex;
};

class PageCache;

class PageInfo : public Cacheable {
friend class PageCache;
public:
	typedef int64_t PageNumber;
	
private:
	PageInfo(PageCache *cache, PageNumber number);

	void diskRead();
	
	PageCache *p_cache;
	PageNumber p_number;
	char *p_buffer;
	// true if the page has successfully been read
	bool p_isReady;
	// true if the page should be written
	bool p_isDirty;
	int p_useCount;

	std::vector<TaskCallback> callbacks;
};

class PageCache {
friend class PageInfo;
public:
	typedef int64_t PageNumber;

	PageCache(CacheHost *cache_host, int page_size, TaskPool *io_pool);
	
	void open(const std::string &path);

	void initializePage(PageNumber number,
			Async::Callback<void(char *)> callback);
	void readPage(PageNumber number,
			Async::Callback<void(char *)> callback);
	void writePage(PageNumber number);
	void releasePage(PageNumber number);

	int getPageSize();
	int getUsedCount();

private:
	CacheHost *p_cacheHost;
	int p_pageSize;
	int p_usedCount;
	TaskPool *p_ioPool;
	std::mutex p_mutex;
	std::unique_ptr<Linux::File> p_file;

	std::unordered_map<PageNumber, PageInfo *> p_presentPages;
	
	class ReadClosure {
	public:
		ReadClosure(PageCache *cache, PageNumber page_number,
				Async::Callback<void(char *)> callback);

		void complete();

	private:
		PageCache *p_cache;
		PageNumber p_pageNumber;
		Async::Callback<void(char *)> p_callback;
	};
};

#endif

