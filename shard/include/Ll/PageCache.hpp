
#ifndef D3B_LL_PAGE_CACHE_HPP
#define D3B_LL_PAGE_CACHE_HPP

#include <unordered_map>
#include <mutex>

class CacheHost;

class Cacheable {
friend class CacheHost;
public:
	Cacheable();
	
	// called when the resources of this item may be acquired
	virtual void acquire() = 0;
	// called when the resources of this item must be released
	// can only be called after acquire()
	virtual void release() = 0;

	virtual int64_t getFootprint() = 0;

private:
	Cacheable *p_moreRecentlyUsed;
	Cacheable *p_lessRecentlyUsed;
	bool p_alive;
};

class CacheHost {
public:
	CacheHost();
	
	// requests permission to acquire the resources for an item
	void requestAcquire(Cacheable *item);
	// informs the cache host that an item has been accessed
	void onAccess(Cacheable *item);
	// signals that the resources belonging to an item have been successfully released
	void afterRelease(Cacheable *item);
	
	void setLimit(int64_t limit);

private:
	void releaseItem(Cacheable *item,
			std::unique_lock<std::mutex> &lock);

	Cacheable *&mostRecently();
	Cacheable *&leastRecently();

	class Sentinel : public Cacheable {
	public:
		virtual void acquire();
		virtual void release();
		virtual int64_t getFootprint();
	};

	std::mutex p_listMutex;
	std::deque<Cacheable *> p_acquireFifo;
	Sentinel p_sentinel;
	int64_t p_activeFootprint;
	int64_t p_limit;
};

class PageCache;

class PageInfo : public Cacheable {
friend class PageCache;
public:
	typedef int64_t PageNumber;

	enum Flags {
		kFlagLoaded = 1,
		kFlagDirty = 2,
		kFlagInitialize = 4,
		kFlagRelease = 8
	};
	
	virtual void acquire();
	virtual void release();
	virtual int64_t getFootprint();
	
private:
	PageInfo(PageCache *cache, PageNumber number);

	void diskRead();
	void doRelease(std::unique_lock<std::mutex> lock);
	
	PageCache *p_cache;
	char *p_buffer;
	PageNumber p_number;
	std::vector<TaskCallback> p_waitQueue;
	int p_useCount;
	uint8_t p_flags;
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

