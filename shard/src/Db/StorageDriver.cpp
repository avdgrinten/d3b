
#include <iostream>

#include "Os/Linux.h"

#include "Db/StorageDriver.hpp"

namespace Db {

void StorageDriver::submitUpdate(Proto::Update &update,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kUpdate;
	queued.update = &update;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

void StorageDriver::submitFix(Proto::Update &update,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kFix;
	queued.update = &update;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

void StorageDriver::submitCommit(Proto::Update &update,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kCommit;
	queued.update = &update;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
}

void StorageDriver::process() {
	if(p_submitQueue.empty())
		return;
	
	Queued queued = p_submitQueue.front();
	p_submitQueue.pop_front();
	if(queued.type == Queued::kUpdate) {
		std::cout << "process update" << std::endl;
//		updateAccept(*queued.update, queued.callback);
	}else if(queued.type == Queued::kFix) {
		/* TODO */
	}else if(queued.type == Queued::kCommit) {
		std::cout << "process commit" << std::endl;
//		processUpdate(*queued.update, queued.callback);
	}else throw std::logic_error("Queued item has illegal type");
}

} /* namespace Db  */

