
#include <unordered_map>

class PageCache {
public:
	typedef int64_t PageNumber;

	PageCache(int page_size);
	
	void open(const std::string &path);

	char *initializePage(PageNumber number);
	void readPage(PageNumber number,
			Async::Callback<void(char *)> callback);
	void writePage(PageNumber number);
	void releasePage(PageNumber number);

	int getUsedCount();

private:
	struct PageInfo {
		char *buffer;
		bool isDirty;
		int useCount;
	};

	std::unique_ptr<Linux::File> p_file;

	std::unordered_map<PageNumber, PageInfo> p_presentPages;
	int p_pageSize;
	int p_usedCount;
};

