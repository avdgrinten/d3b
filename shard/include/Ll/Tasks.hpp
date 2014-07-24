
#include <mutex>

class LocalTaskQueue {
public:
	static void set(LocalTaskQueue *pointer);
	static LocalTaskQueue *get();

	LocalTaskQueue();

	void submit(Async::Callback<void()> callback);

	void process();

private:
	std::mutex p_mutex;
	std::unique_ptr<Linux::EventFd> p_eventFd;
	std::queue<Async::Callback<void()>> p_queue;
};

