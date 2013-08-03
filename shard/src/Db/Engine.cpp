
#include <iostream>
#include <functional>

#include "Async.h"
#include "Os/Linux.h"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

StorageRegistry globStorageRegistry;
ViewRegistry globViewRegistry;

Engine::Engine() : p_nextTransactId(1) {
	p_storage.push_back(nullptr);
	p_views.push_back(nullptr);
}

void Engine::createConfig() {
	if(osIntf->fileExists(p_path + "/config"))
		throw std::runtime_error("Config file exists already!");
	osIntf->mkDir(p_path + "/storage");
	osIntf->mkDir(p_path + "/views");
	osIntf->mkDir(p_path + "/extern");
	p_writeConfig();
}

void Engine::loadConfig() {
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(p_path + "/config", Linux::FileMode::read);
	Linux::size_type length = file->lengthSync();
	char *buffer = new char[length];
	file->preadSync(0, length, buffer);
	file->closeSync();
	
	Proto::Config config;
	config.ParseFromArray(buffer, length);
	delete[] buffer;
	
	for(int i = 0; i < config.storage_size(); i++) {
		const Proto::StorageConfig &storage_config = config.storage(i);
		StorageDriver *instance = p_setupStorage(storage_config);
		instance->loadStorage();
	}
	
	for(int i = 0; i < config.views_size(); i++) {
		const Proto::ViewConfig &view_config = config.views(i);
		ViewDriver *instance = p_setupView(view_config);
		instance->loadView();
	}
}

void Engine::createStorage(const Proto::StorageConfig &config) {
	if(getStorage(config.identifier()) != -1)
		throw std::runtime_error("Storage exists already!");
	osIntf->mkDir(p_path + "/storage/" + config.identifier());
	StorageDriver *instance = p_setupStorage(config);
	instance->createStorage();
	p_writeConfig();
}
int Engine::getStorage(const std::string &identifier) {
	for(int i = 1; i < p_storage.size(); i++) {
		if(p_storage[i] == nullptr)
			continue;
		if(p_storage[i]->getIdentifier() == identifier)
			return i;
	}
	return -1;
}
void Engine::unlinkStorage(int storage) {
	if(storage == -1)
		throw std::runtime_error("Illegal storage specified");
	StorageDriver *driver = p_storage[storage];
	if(driver == nullptr)
		throw std::runtime_error("Requested storage does not exist");
	p_storage[storage] = nullptr;
	p_writeConfig();
}

void Engine::createView(const Proto::ViewConfig &config) {
	if(getView(config.identifier()) != -1)
		throw std::runtime_error("View exists already!");
	osIntf->mkDir(p_path + "/views/" + config.identifier());
	ViewDriver *instance = p_setupView(config);
	instance->createView();
	p_writeConfig();
}
int Engine::getView(const std::string &identifier) {
	for(int i = 1; i < p_views.size(); i++) {
		if(p_views[i] == nullptr)
			continue;
		if(p_views[i]->getIdentifier() == identifier)
			return i;
	}
	return -1;
}
void Engine::unlinkView(int view) {
	if(view == -1)
		throw std::runtime_error("Illegal view specified");
	ViewDriver *driver = p_views[view];
	if(driver == nullptr)
		throw std::runtime_error("Requested view does not exist");
	p_views[view] = nullptr;
	p_writeConfig();
}

void Engine::beginTransact(std::function<void(Error, trid_type)> callback) {
	trid_type trid = p_nextTransactId++;

	Transaction *transaction = new Transaction;
	p_transacts.insert(std::make_pair(trid, transaction));
	
	callback(Error(true), trid);
}
void Engine::update(trid_type trid, Proto::Update *update,
		std::function<void(Error)> callback) {
	/* TODO: move this to process() ? */
	auto transact_it = p_transacts.find(trid);
	if(transact_it == p_transacts.end())
		throw std::runtime_error("Illegal transaction");
	
	/* remember the update objects as we will need them
		during commit */
	Transaction *transaction = transact_it->second;
	transaction->updates.push_back(update);

	/* TODO: write updates to the write-ahead log */

	/* submit each update to its associated storage */	
	int storage = getStorage(update->storage_name());
	if(storage == -1)
		throw std::runtime_error("Illegal storage specified");

	StorageDriver *driver = p_storage[storage];
	driver->submitUpdate(update, callback);
}
void Engine::commit(trid_type trid,
		std::function<void(Error)> callback) {
	/* TODO: move this to process() ? */
	auto transact_it = p_transacts.find(trid);
	if(transact_it == p_transacts.end())
		throw std::runtime_error("Illegal transaction");
	
	/* TODO: write commit to the write-ahead log */
	
	/* commit each update to its associated storage */
	Transaction *transaction = transact_it->second;
	Async::eachSeries(transaction->updates.begin(),
			transaction->updates.end(),
			[=] (Proto::Update *update, std::function<void()> each_callback) {
		int storage = getStorage(update->storage_name());
		if(storage == -1)
			throw std::runtime_error("Illegal storage specified");

		StorageDriver *driver = p_storage[storage];
		driver->submitCommit(update, [=](Error error) { each_callback(); });
	}, [=]() { callback(Error(true)); }); /*FIXME: proper error value */
}

void Engine::process() {
	for(int i = 1; i < p_storage.size(); i++) {
		if(p_storage[i] == nullptr)
			continue;
		while(p_storage[i]->queuePending())
			p_storage[i]->processQueue();
	}
}

void Engine::onUpdate(Proto::Update *update,
		std::function<void(Error)> callback) {
	Async::eachSeries(p_views.begin(), p_views.end(),
			[=](ViewDriver *driver, std::function<void()> each_cb) {
		if(driver == nullptr)
			return each_cb();
		driver->onUpdate(update, [=](Error error) { each_cb(); });
	}, [=]() { callback(Error(true)); }); /* FIXME: proper error value */
}

void Engine::fetch(Proto::Fetch *fetch,
		std::function<void(Proto::FetchData &)> on_data,
		std::function<void(Error)> callback) {
	if(!fetch->has_storage_idx()) {
		int storage = getStorage(fetch->storage_name());
		if(storage == -1)
			throw std::runtime_error("Illegal storage specified");
		fetch->set_storage_idx(storage);
	}
	
	StorageDriver *driver = p_storage[fetch->storage_idx()];
	driver->submitFetch(fetch, on_data, callback);
}

Error Engine::query(const Proto::Query &request,
		std::function<void(const Proto::Rows&)> report,
		std::function<void()> callback) {
	int view = -1;
	if(request.has_view_name())
		view = getView(request.view_name());
	if(view == -1)
		return Error(kErrIllegalView);
	ViewDriver *driver = p_views[view];
	driver->query(request, report, callback);
}

StorageDriver *Engine::p_setupStorage(const Proto::StorageConfig &config) {
	StorageDriver::Factory *factory
			= globStorageRegistry.getDriver(config.driver());
	if(factory == nullptr)
		throw std::runtime_error(std::string("Storage driver '")
			+ config.driver() + std::string("' not available"));
	std::cout << "Setting up storage '" << config.identifier() << "'" << std::endl;

	StorageDriver *instance = factory->newDriver(this);
	instance->setIdentifier(config.identifier());
	instance->setPath(p_path + "/storage/" + config.identifier());
	instance->readConfig(config);
	p_storage.push_back(instance);
	return instance;
}

ViewDriver *Engine::p_setupView(const Proto::ViewConfig &config) {
	ViewDriver::Factory *factory
			= globViewRegistry.getDriver(config.driver());
	if(factory == nullptr)
		throw std::runtime_error(std::string("View driver '")
			+ config.driver() + std::string("' not available"));
	std::cout << "Setting up view '" << config.identifier() << "'" << std::endl;

	ViewDriver *instance = factory->newDriver(this);
	instance->setIdentifier(config.identifier());
	instance->setPath(p_path + "/views/" + config.identifier());
	instance->readConfig(config);
	p_views.push_back(instance);
	return instance;
}

void Engine::p_writeConfig() {
	Proto::Config config;
	
	for(int i = 1; i < p_storage.size(); i++) {
		if(p_storage[i] == nullptr)
			continue;
		*config.add_storage() = p_storage[i]->writeConfig();
	}
	for(int i = 1; i < p_views.size(); i++) {
		if(p_views[i] == nullptr)
			continue;
		*config.add_views() = p_views[i]->writeConfig();
	}

	std::string serialized = config.SerializeAsString();
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(p_path + "/config", Linux::kFileWrite
			| Linux::kFileCreate | Linux::kFileTrunc);
	file->pwriteSync(0, serialized.size(), serialized.c_str());
	file->closeSync();
}

};

