
#ifndef D3B_LL_RANDOM_ACCESS_FILE_HPP
#define D3B_LL_RANDOM_ACCESS_FILE_HPP

#include "ll/page-cache.hpp"

namespace Ll {

// implements a random access interface on top of a PageCache
class RandomAccessFile {
public:
	RandomAccessFile(const std::string &name,
			CacheHost *cache_host, TaskPool *io_pool);

	void setPath(const std::string &path);
	
	void createFile();

	class ReadClosure {
	public:
		ReadClosure(RandomAccessFile *file);

		void read(int64_t offset, int64_t length,
				char *buffer, Async::Callback<void()> callback);

	private:
		void fetchPage();
		void onPageLoad(char *page_buffer);

		RandomAccessFile *p_file;
		int64_t p_position;
		int64_t p_length;
		char *p_buffer;
		Async::Callback<void()> p_callback;

		int64_t p_offset;
	};
	class WriteClosure {
	public:
		WriteClosure(RandomAccessFile *file);

		void write(int64_t offset, int64_t length,
				const char *buffer, Async::Callback<void()> callback);

	private:
		void fetchPage();
		void onPageLoad(char *page_buffer);

		RandomAccessFile *p_file;
		int64_t p_position;
		int64_t p_length;
		const char *p_buffer;
		Async::Callback<void()> p_callback;

		int64_t p_offset;
	};

private:
	PageCache p_pageCache;
	std::string p_path;
	std::string p_name;
};

} // namespace Ll

#endif

