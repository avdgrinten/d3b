
#include <iostream>
#include <functional>

#include "Os/Linux.h"
#include "Async.h"
#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

StorageRegistry globStorageRegistry;
ViewRegistry globViewRegistry;

Engine::Engine() : p_nextTransactId(1) {
	p_storage.push_back(nullptr);
	p_views.push_back(nullptr);

	p_eventFd = osIntf->createEventFd();
}

void Engine::createConfig() {
	if(osIntf->fileExists(p_path + "/config"))
		throw std::runtime_error("Config file exists already!");
	osIntf->mkDir(p_path + "/storage");
	osIntf->mkDir(p_path + "/views");
	osIntf->mkDir(p_path + "/extern");
	
	p_writeAhead.setPath(p_path);
	p_writeAhead.setIdentifier("transact");
	p_writeAhead.createLog();
	
	p_writeConfig();
}

void Engine::loadConfig() {
	p_writeAhead.setPath(p_path);
	p_writeAhead.setIdentifier("transact");
	p_writeAhead.loadLog();
	
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

void Engine::transaction(std::function<void(Error, trid_type)> callback) {
	trid_type trid = p_nextTransactId++;

	Transaction *transaction = new Transaction;
	p_openTransactions.insert(std::make_pair(trid, transaction));
	callback(Error(true), trid);
}
void Engine::updateMutation(trid_type trid, Mutation *mutation,
		std::function<void(Error)> callback) {
	auto transact_it = p_openTransactions.find(trid);
	if(transact_it == p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");
		
	Transaction *transaction = transact_it->second;
	transaction->mutations.push_back(mutation);
	callback(Error(true));
}
void Engine::submit(trid_type trid,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kTypeSubmit;
	queued.trid = trid;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->fire();
}
void Engine::commit(trid_type trid,
		std::function<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kTypeCommit;
	queued.trid = trid;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->fire();
}

void Engine::process() {
	Async::whilst([=]() -> bool { return true; },
			[=](std::function<void(Error)> main_cb) {
		Async::staticSeries(std::make_tuple(
			[=](std::function<void(Error)> ser_cb) {
				p_processQueue(ser_cb);
			},
			[=](std::function<void(Error)> ser_cb) {
				p_eventFd->onEvent([=]() {
					p_processQueue(ser_cb);
				});
				p_eventFd->install();
			},
			[=](std::function<void(Error)> ser_cb) {
				p_eventFd->uninstall();
				ser_cb(Error(true));
			}
		), [=](Error error) {
			main_cb(error);
		});
	}, [=](Error error) {
		std::cout << "fin" << std::endl;
	});
}

void Engine::p_processQueue(std::function<void(Error)> callback) {
	Async::whilst([=]() -> bool { return !p_submitQueue.empty(); },
		[=](std::function<void(Error)> main_cb) {
			Queued queued = p_submitQueue.front();
			p_submitQueue.pop_front();
			if(queued.type == Queued::kTypeSubmit) {
				auto transact_it = p_openTransactions.find(queued.trid);
				if(transact_it == p_openTransactions.end())
					throw std::runtime_error("Illegal transaction");
				Transaction *transaction = transact_it->second;
				
				Proto::LogEntry log_entry;
				log_entry.set_type(Proto::LogEntry::kTypeSubmit);
				log_entry.set_transaction_id(queued.trid);

				for(auto it = transaction->mutations.begin();
						it != transaction->mutations.end(); ++it) {
					Mutation *mutation = *it;

					Proto::LogMutation *log_mutation = log_entry.add_mutations();
					if(mutation->type == Mutation::kTypeInsert) {
						StorageDriver *driver = p_storage[mutation->storageIndex];
						
						log_mutation->set_type(Proto::LogMutation::kTypeInsert);
						log_mutation->set_storage_name(driver->getIdentifier());
						log_mutation->set_buffer(mutation->buffer);
					}else if(mutation->type == Mutation::kTypeModify) {
						StorageDriver *driver = p_storage[mutation->storageIndex];
						
						log_mutation->set_type(Proto::LogMutation::kTypeModify);
						log_mutation->set_storage_name(driver->getIdentifier());
						log_mutation->set_document_id(mutation->documentId);
						log_mutation->set_buffer(mutation->buffer);
					}else throw std::logic_error("Illegal mutation type");
				}
			
				p_writeAhead.log(log_entry, [queued, main_cb](Error error) {
					//TODO: handle failure
					//FIXME: this would segfault if p_writeAhead.write() was truly asyncronous

					queued.callback(Error(true));
					osIntf->nextTick([=]() { main_cb(Error(true)); });
				});
			}else if(queued.type == Queued::kTypeCommit) {
				auto transact_it = p_openTransactions.find(queued.trid);
				if(transact_it == p_openTransactions.end())
					throw std::runtime_error("Illegal transaction");
				Transaction *transaction = transact_it->second;
				
				Proto::LogEntry log_entry;
				log_entry.set_type(Proto::LogEntry::kTypeCommit);
				log_entry.set_transaction_id(queued.trid);

				p_writeAhead.log(log_entry, [this, queued, transact_it, transaction, main_cb](Error error) {
					//TODO: handle failure
					//FIXME: this would segfault if p_writeAhead.write() was truly asyncronous

					for(auto it = p_storage.begin(); it != p_storage.end(); ++it) {
						StorageDriver *driver = *it;
						if(driver == nullptr)
							continue;
						driver->sequence(transaction->mutations);
					}
					for(auto it = p_views.begin(); it != p_views.end(); ++it) {
						ViewDriver *driver = *it;
						if(driver == nullptr)
							continue;
						driver->sequence(transaction->mutations);
					}
					
					// commit/rollback always frees the transaction
					p_openTransactions.erase(transact_it);
					delete transaction;
					
					queued.callback(Error(true));

					osIntf->nextTick([=]() { main_cb(Error(true)); });
				});
			}else throw std::logic_error("Queued item has illegal type");
		}, callback);
}

void Engine::fetch(FetchRequest *fetch,
		std::function<void(FetchData &)> on_data,
		std::function<void(Error)> callback) {
	StorageDriver *driver = p_storage[fetch->storageIndex];
	driver->processFetch(fetch, on_data, callback);
}

Error Engine::query(Query *request,
		std::function<void(QueryData &)> on_data,
		std::function<void(Error)> callback) {
	ViewDriver *driver = p_views[request->viewIndex];
	driver->processQuery(request, on_data, callback);
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

