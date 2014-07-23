

#include <iostream>
#include <functional>

#include "Os/Linux.hpp"
#include "Async.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

QueuedViewDriver::QueuedViewDriver(Engine *engine)
		: ViewDriver(engine), p_currentSequenceId(0) {
	p_eventFd = osIntf->createEventFd();
}

void QueuedViewDriver::processQueue() {
	auto closure = new ProcessClosure(this);
	closure->process();
}

void QueuedViewDriver::sequence(SequenceId sequence_id,
		std::vector<Mutation> &mutations,
		Async::Callback<void()> callback) {
	SequenceQueueItem item;
	item.sequenceId = sequence_id;
	item.mutations = &mutations;
	item.callback = callback;
	p_sequenceQueue.push(item);
	p_eventFd->increment();
}

void QueuedViewDriver::query(Query *query,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(Error)> callback) {
	QueryQueueItem item;
	item.query = query;
	item.onData = on_data;
	item.callback = callback;
	p_queryQueue.push(item);
	p_eventFd->increment();
}

// --------------------------------------------------------
// ProcessClosure
// --------------------------------------------------------

QueuedViewDriver::ProcessClosure::ProcessClosure(QueuedViewDriver *view)
		: p_view(view) { }

void QueuedViewDriver::ProcessClosure::process() {
	if(!p_view->p_sequenceQueue.empty()) {
		p_sequenceItem = p_view->p_sequenceQueue.front();
		p_view->p_sequenceQueue.pop();

		p_index = 0;
		processSequence();
	}else if(!p_view->p_queryQueue.empty()) {
		p_queryItem = p_view->p_queryQueue.front();
		p_view->p_queryQueue.pop();

		p_view->processQuery(p_queryItem.query, p_queryItem.onData,
				ASYNC_MEMBER(this, &ProcessClosure::onQueryComplete));
	}else{
		p_view->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessClosure::process));
	}
}

void QueuedViewDriver::ProcessClosure::processSequence() {
	if(p_index == p_sequenceItem.mutations->size()) {
		p_view->p_currentSequenceId = p_sequenceItem.sequenceId;
		osIntf->nextTick(ASYNC_MEMBER(this, &ProcessClosure::process));
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
void QueuedViewDriver::ProcessClosure::onQueryComplete(Error error) {
	//FIXME: don't ignore error
	p_queryItem.callback(Error(true));

	osIntf->nextTick(ASYNC_MEMBER(this, &ProcessClosure::process));
}

} /* namespace Db  */

