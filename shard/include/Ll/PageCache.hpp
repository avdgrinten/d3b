
#ifndef D3B_LL_PAGE_CACHE_HPP
#define D3B_LL_PAGE_CACHE_HPP

#include <unordered_map>
#include <mutex>

class PageCache {
public:
	typedef int64_t PageNumber;

	PageCache(int page_size, TaskPool *io_pool);
	
	void open(const std::string &path);

	char *initializePage(PageNumber number);
	void readPage(PageNumber number,
			Async::Callback<void(char *)> callback);
	void writePage(PageNumber number);
	void releasePage(PageNumber number);

	int getPageSize();
	int getUsedCount();

private:
	struct PageInfo {
		char *buffer;
		// true if the page has successfully been read
		bool isReady;
		// true if the page should be written
		bool isDirty;
		int useCount;

		std::vector<TaskCallback> callbacks;
	};

	int p_pageSize;
	int p_usedCount;
	TaskPool *p_ioPool;
	std::mutex p_mutex;
	std::unique_ptr<Linux::File> p_file;

	std::unordered_map<PageNumber, PageInfo> p_presentPages;

	class SubmitClosure {
	public:
		SubmitClosure(PageCache *cache, PageNumber page_number);

		void process();

	private:
		PageCache *p_cache;
		PageNumber p_pageNumber;
	};

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

