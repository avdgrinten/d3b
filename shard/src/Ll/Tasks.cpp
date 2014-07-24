
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"

#include "Ll/Tasks.hpp"

__thread LocalTaskQueue *localQueuePointer = nullptr;

void LocalTaskQueue::set(LocalTaskQueue *pointer) {
	localQueuePointer = pointer;
}
LocalTaskQueue *LocalTaskQueue::get() {
	if(localQueuePointer == nullptr)
		throw std::logic_error("No LocalTaskQueue defined");
	return localQueuePointer;
}

LocalTaskQueue::LocalTaskQueue() {
	p_eventFd = osIntf->createEventFd();
}

void LocalTaskQueue::submit(Async::Callback<void()> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	p_queue.push(callback);
	lock.unlock();
	
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

