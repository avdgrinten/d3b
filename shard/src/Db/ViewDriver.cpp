

#include <iostream>
#include <functional>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

QueuedViewDriver::QueuedViewDriver(Engine *engine)
		: ViewDriver(engine), p_currentSequenceId(0),
		p_activeRequests(0), p_requestPhase(true) {
	p_eventFd = osIntf->createEventFd();
}

void QueuedViewDriver::processQueue() {
	auto closure = new ProcessClosure(this);
	closure->process();
}

void QueuedViewDriver::sequence(SequenceId sequence_id,
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

void QueuedViewDriver::query(QueryRequest *query,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(QueryError)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	if(p_requestPhase) {
		p_activeRequests++;
		
		lock.unlock();
		processQuery(query, on_data, callback);
	}else{
		QueryQueueItem item;
		item.query = query;
		item.onData = on_data;
		item.callback = callback;
		p_queryQueue.push(item);

		lock.unlock();
	}

	p_eventFd->increment();
}

void QueuedViewDriver::finishRequest() {
	std::lock_guard<std::mutex> lock(p_mutex);

	p_activeRequests--;
}

// --------------------------------------------------------
// ProcessClosure
// --------------------------------------------------------

QueuedViewDriver::ProcessClosure::ProcessClosure(QueuedViewDriver *view)
		: p_view(view) { }

void QueuedViewDriver::ProcessClosure::process() {
	std::unique_lock<std::mutex> lock(p_view->p_mutex);

	if(!p_view->p_sequenceQueue.empty()) {
		p_view->p_requestPhase = false;
		
		lock.unlock();
		endRequestPhase();
	}else{
		lock.unlock();
		p_view->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::process));
	}
}

void QueuedViewDriver::ProcessClosure::endRequestPhase() {
	std::unique_lock<std::mutex> lock(p_view->p_mutex);

	if(p_view->p_activeRequests == 0) {
		lock.unlock();
		sequencePhase();
	}else{
		lock.unlock();
		p_view->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::endRequestPhase));
	}
}

void QueuedViewDriver::ProcessClosure::sequencePhase() {
	std::unique_lock<std::mutex> lock(p_view->p_mutex);

	if(p_view->p_sequenceQueue.empty()) {
		p_view->p_requestPhase = true;
		
		TaskPool *pool = p_view->getEngine()->getProcessPool();
		for(int i = 0; i < p_view->p_queryQueue.size() + 1; i++)
			pool->submit(ASYNC_MEMBER(this, &ProcessClosure::unqueueRequest));
	}else{
		p_sequenceItem = p_view->p_sequenceQueue.front();
		p_view->p_sequenceQueue.pop();

		lock.unlock();
		p_index = 0;
		processSequence();
	}
}

void QueuedViewDriver::ProcessClosure::unqueueRequest() {
	std::unique_lock<std::mutex> lock(p_view->p_mutex);
	
	if(p_view->p_queryQueue.empty()) {
		lock.unlock();
		process();
	}else{
		auto query_item = p_view->p_queryQueue.front();
		p_view->p_queryQueue.pop();

		p_view->p_activeRequests++;
			
		lock.unlock();
		p_view->processQuery(query_item.query, query_item.onData, query_item.callback);
	}
}

void QueuedViewDriver::ProcessClosure::processSequence() {
	if(p_index == p_sequenceItem.mutations->size()) {
		p_view->p_currentSequenceId = p_sequenceItem.sequenceId;
		p_sequenceItem.callback();
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessClosure::sequencePhase));
		return;
	}
	
	Mutation &mutation = (*p_sequenceItem.mutations)[p_index];
	if(mutation.type == Mutation::kTypeInsert) {
		p_view->processInsert(p_sequenceItem.sequenceId,
				mutation, ASYNC_MEMBER(this, &ProcessClosure::onSequenceItem));
	}else if(mutation.type == Mutation::kTypeModify) {
		p_view->processModify(p_sequenceItem.sequenceId,
				mutation, ASYNC_MEMBER(this, &ProcessClosure::onSequenceItem));
	}else throw std::logic_error("Illegal mutation type");
}
void QueuedViewDriver::ProcessClosure::onSequenceItem(Error error) {
	//FIXME: don't ignore error
	p_index++;
	processSequence();
}

} /* namespace Db  */

