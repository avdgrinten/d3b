
#include <iostream>
#include <functional>

#include "Os/Linux.h"
#include "Async.h"

#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

void StorageDriver::submitUpdate(Proto::Update *update,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kUpdate;
	queued.update = update;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

void StorageDriver::submitCommit(Proto::Update *update,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kCommit;
	queued.update = update;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

void StorageDriver::submitFetch(Proto::Fetch *fetch,
		std::function<void(Proto::FetchData &)> on_data,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kFetch;
	queued.fetch = fetch;
	queued.onFetchData = on_data;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

bool StorageDriver::queuePending() {
	return !p_submitQueue.empty();
}
void StorageDriver::processQueue() {
	if(p_submitQueue.empty())
		return;
	
	Queued queued = p_submitQueue.front();
	p_submitQueue.pop_front();
	if(queued.type == Queued::kUpdate) {
		std::cout << "process update" << std::endl;
		updateAccept(queued.update, queued.callback);
		/* TODO: fix the update */
	}else if(queued.type == Queued::kCommit) {
		std::cout << "process commit" << std::endl;
		Async::staticSeries(std::make_tuple(
			[=](std::function<void()> cb) {
				processUpdate(queued.update, [=](Error error) {
					cb();
				});
			},
			[=](std::function<void()> cb) {
				Engine *engine = getEngine();
				engine->onUpdate(queued.update, [=](Error error) {
					cb();
				});
			}
		), [=]() {
			queued.callback(Error(true)); /* FIXME: proper error value */
		});
	}else if(queued.type == Queued::kFetch) {
		processFetch(queued.fetch, queued.onFetchData, queued.callback);
	}else throw std::logic_error("Queued item has illegal type");
}

} /* namespace Db  */

