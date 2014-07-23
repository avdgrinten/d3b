
#include <iostream>
#include <functional>

#include "Os/Linux.hpp"
#include "Async.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

QueuedStorageDriver::QueuedStorageDriver(Engine *engine)
		: StorageDriver(engine), p_currentSequenceId(0) {
	p_eventFd = osIntf->createEventFd();
}

void QueuedStorageDriver::processQueue() {
	auto closure = new ProcessClosure(this);
	closure->process();
}

void QueuedStorageDriver::sequence(SequenceId sequence_id,
		std::vector<Mutation> &mutations,
		Async::Callback<void()> callback) {
	SequenceQueueItem item;
	item.sequenceId = sequence_id;
	item.mutations = &mutations;
	item.callback = callback;
	p_sequenceQueue.push(item);
	p_eventFd->increment();
}

void QueuedStorageDriver::fetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(Error)> callback) {
	FetchQueueItem item;
	item.fetch = fetch;
	item.onData = on_data;
	item.callback = callback;
	p_fetchQueue.push(item);
	p_eventFd->increment();
}

// --------------------------------------------------------
// ProcessClosure
// --------------------------------------------------------

QueuedStorageDriver::ProcessClosure::ProcessClosure(QueuedStorageDriver *storage)
		: p_storage(storage) { }

void QueuedStorageDriver::ProcessClosure::process() {
	if(!p_storage->p_sequenceQueue.empty()) {
		p_sequenceItem = p_storage->p_sequenceQueue.front();
		p_storage->p_sequenceQueue.pop();

		p_index = 0;
		processSequence();
	}else if(!p_storage->p_fetchQueue.empty()) {
		p_fetchItem = p_storage->p_fetchQueue.front();
		p_storage->p_fetchQueue.pop();

		p_storage->processFetch(p_fetchItem.fetch, p_fetchItem.onData,
				ASYNC_MEMBER(this, &ProcessClosure::onFetchComplete));
	}else{
		p_storage->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::process));
	}
}

void QueuedStorageDriver::ProcessClosure::processSequence() {
	if(p_index == p_sequenceItem.mutations->size()) {
		p_storage->p_currentSequenceId = p_sequenceItem.sequenceId;
		osIntf->nextTick(ASYNC_MEMBER(this, &ProcessClosure::process));
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
void QueuedStorageDriver::ProcessClosure::onFetchComplete(Error error) {
	//FIXME: don't ignore error
	p_fetchItem.callback(Error(true));

	osIntf->nextTick(ASYNC_MEMBER(this, &ProcessClosure::process));
}

} /* namespace Db  */

