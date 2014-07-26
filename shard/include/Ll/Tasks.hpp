
#include <random>
#include <mutex>
#include <thread>

class LocalTaskQueue {
public:
	static void set(LocalTaskQueue *pointer);
	static LocalTaskQueue *get();

	LocalTaskQueue(OS::LocalAsyncHost *async_host);

	void submit(Async::Callback<void()> callback);
	void wake();

	void process();

private:
	std::mutex p_mutex;
	std::unique_ptr<Linux::EventFd> p_eventFd;
	std::queue<Async::Callback<void()>> p_queue;
};

class TaskCallback {
public:
	TaskCallback(Async::Callback<void()> callback);
	TaskCallback(LocalTaskQueue *queue, Async::Callback<void()> callback);

	void operator() ();

private:
	LocalTaskQueue *p_queue;
	Async::Callback<void()> p_callback;
};

class TaskPool {
public:
	void addWorker(LocalTaskQueue *worker);

	void submit(Async::Callback<void()> callback);
	
private:
	std::vector<LocalTaskQueue *> p_workers;
	std::minstd_rand p_randomEngine;
};

class WorkerThread {
public:
	WorkerThread();

	void shutdown();

	std::thread &getThread();
	LocalTaskQueue *getTaskQueue();

private:
	void threadMain();

	OS::LocalAsyncHost *p_asyncHost;
	LocalTaskQueue *p_taskQueue;
	std::thread p_thread;

	volatile bool p_shutdown;
};

