
#include <cassert>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"

#include "Ll/Tasks.hpp"

// --------------------------------------------------------
// LocalTaskQueue
// --------------------------------------------------------

__thread LocalTaskQueue *localQueuePointer = nullptr;

void LocalTaskQueue::set(LocalTaskQueue *pointer) {
	localQueuePointer = pointer;
}
LocalTaskQueue *LocalTaskQueue::get() {
	if(localQueuePointer == nullptr)
		throw std::logic_error("No LocalTaskQueue defined");
	return localQueuePointer;
}

LocalTaskQueue::LocalTaskQueue(OS::LocalAsyncHost *async_host) {
	p_eventFd = osIntf->createEventFd(async_host);
}

void LocalTaskQueue::submit(Async::Callback<void()> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	p_queue.push(callback);
	lock.unlock();
	
	p_eventFd->increment();
}

void LocalTaskQueue::wake() {
	p_eventFd->increment();
}

void LocalTaskQueue::process() {
	std::unique_lock<std::mutex> lock(p_mutex);
	while(!p_queue.empty()) {
		Async::Callback<void()> callback = p_queue.front();
		p_queue.pop();
		lock.unlock();
		
		callback();

		lock.lock();
	}
	lock.unlock();

	p_eventFd->wait(ASYNC_MEMBER(this, &LocalTaskQueue::process));
}

// --------------------------------------------------------
// TaskCallback
// --------------------------------------------------------

TaskCallback::TaskCallback(Async::Callback<void()> callback)
	: p_queue(LocalTaskQueue::get()), p_callback(callback) { }

TaskCallback::TaskCallback(LocalTaskQueue *queue, Async::Callback<void()> callback)
	: p_queue(queue), p_callback(callback) { }

void TaskCallback::operator() () {
	p_queue->submit(p_callback);
}

// --------------------------------------------------------
// TaskPool
// --------------------------------------------------------

void TaskPool::addWorker(LocalTaskQueue *queue) {
	p_workers.push_back(queue);
}

void TaskPool::submit(Async::Callback<void()> callback) {
	assert(p_workers.size() > 0);
	std::uniform_int_distribution<> distrib(0, p_workers.size() - 1);
	int index = distrib(p_randomEngine);
	p_workers[index]->submit(callback);
}

// --------------------------------------------------------
// WorkerThread
// --------------------------------------------------------

WorkerThread::WorkerThread() : p_shutdown(false) {
	p_asyncHost = new OS::LocalAsyncHost();
	p_taskQueue = new LocalTaskQueue(p_asyncHost);

	p_thread = std::thread(ASYNC_MEMBER(this, &WorkerThread::threadMain));
}

void WorkerThread::shutdown() {
	p_shutdown = true;
	p_taskQueue->wake();
}

LocalTaskQueue *WorkerThread::getTaskQueue() {
	return p_taskQueue;
}

std::thread &WorkerThread::getThread() {
	return p_thread;
}

void WorkerThread::threadMain() {
	OS::LocalAsyncHost::set(p_asyncHost);
	LocalTaskQueue::set(p_taskQueue);

	p_taskQueue->process();

	while(!p_shutdown)
		p_asyncHost->process();
}

