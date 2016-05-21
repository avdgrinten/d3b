
#include <iostream>
#include <functional>

#include "async.hpp"
#include "os/linux.hpp"
#include "ll/tasks.hpp"

#include "db/types.hpp"
#include "db/storage-driver.hpp"
#include "db/view-driver.hpp"
#include "db/engine.hpp"

namespace Db {

QueuedStorageDriver::QueuedStorageDriver(Engine *engine)
		: StorageDriver(engine), p_currentSequenceId(0),
		p_activeRequests(0), p_requestPhase(true) {
	p_eventFd = osIntf->createEventFd();
}

void QueuedStorageDriver::processQueue() {
	auto closure = new ProcessClosure(this);
	closure->process();
}

void QueuedStorageDriver::sequence(SequenceId sequence_id,
		std::vector<Mutation> &mutations,
		Async::Callback<void()> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	SequenceQueueItem item;
	item.sequenceId = sequence_id;
	item.mutations = &mutations;
	item.callback = callback;
	p_sequenceQueue.push(item);
	
	lock.unlock();
	p_eventFd->increment();
}

void QueuedStorageDriver::fetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(FetchError)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	if(p_requestPhase) {
		p_activeRequests++;

		lock.unlock();
		processFetch(fetch, on_data, callback);
	}else{
		FetchQueueItem item;
		item.fetch = fetch;
		item.onData = on_data;
		item.callback = callback;
		p_fetchQueue.push(item);

		lock.unlock();
	}

	p_eventFd->increment();
}

void QueuedStorageDriver::finishRequest() {
	std::lock_guard<std::mutex> lock(p_mutex);

	p_activeRequests--;
}

// --------------------------------------------------------
// ProcessClosure
// --------------------------------------------------------

QueuedStorageDriver::ProcessClosure::ProcessClosure(QueuedStorageDriver *storage)
		: p_storage(storage) { }

void QueuedStorageDriver::ProcessClosure::process() {
	std::unique_lock<std::mutex> lock(p_storage->p_mutex);

	if(!p_storage->p_sequenceQueue.empty()) {
		p_storage->p_requestPhase = false;
		
		lock.unlock();
		endRequestPhase();
	}else{
		lock.unlock();
		p_storage->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::process));
	}
}

void QueuedStorageDriver::ProcessClosure::endRequestPhase() {
	std::unique_lock<std::mutex> lock(p_storage->p_mutex);

	if(p_storage->p_activeRequests == 0) {
		lock.unlock();
		sequencePhase();
	}else{
		lock.unlock();
		p_storage->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::endRequestPhase));
	}
}

void QueuedStorageDriver::ProcessClosure::sequencePhase() {
	std::unique_lock<std::mutex> lock(p_storage->p_mutex);

	if(p_storage->p_sequenceQueue.empty()) {
		p_storage->p_requestPhase = true;
		
		TaskPool *pool = p_storage->getEngine()->getProcessPool();
		for(int i = 0; i < p_storage->p_fetchQueue.size() + 1; i++)
			pool->submit(ASYNC_MEMBER(this, &ProcessClosure::unqueueRequest));
	}else{
		p_sequenceItem = p_storage->p_sequenceQueue.front();
		p_storage->p_sequenceQueue.pop();

		lock.unlock();
		p_index = 0;
		processSequence();
	}
}

void QueuedStorageDriver::ProcessClosure::unqueueRequest() {
	std::unique_lock<std::mutex> lock(p_storage->p_mutex);
	
	if(p_storage->p_fetchQueue.empty()) {
		lock.unlock();
		process();
	}else{
		auto fetch_item = p_storage->p_fetchQueue.front();
		p_storage->p_fetchQueue.pop();
		
		p_storage->p_activeRequests++;

		lock.unlock();
		p_storage->processFetch(fetch_item.fetch, fetch_item.onData,
				fetch_item.callback);
	}
}

void QueuedStorageDriver::ProcessClosure::processSequence() {
	if(p_index == p_sequenceItem.mutations->size()) {
		p_storage->p_currentSequenceId = p_sequenceItem.sequenceId;
		p_sequenceItem.callback();
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessClosure::sequencePhase));
		return;
	}
	
	Mutation &mutation = (*p_sequenceItem.mutations)[p_index];
	if(mutation.type == Mutation::kTypeInsert) {
		p_storage->processInsert(p_sequenceItem.sequenceId,
				mutation, ASYNC_MEMBER(this, &ProcessClosure::onSequenceItem));
	}else if(mutation.type == Mutation::kTypeModify) {
		p_storage->processModify(p_sequenceItem.sequenceId,
				mutation, ASYNC_MEMBER(this, &ProcessClosure::onSequenceItem));
	}else throw std::logic_error("Illegal mutation type");
}
void QueuedStorageDriver::ProcessClosure::onSequenceItem(Error error) {
	//FIXME: don't ignore error
	p_index++;
	processSequence();
}

} /* namespace Db  */

