
#include <iostream>
#include <functional>

#include "Os/Linux.hpp"
#include "Async.hpp"
#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

StorageRegistry globStorageRegistry;
ViewRegistry globViewRegistry;

Engine::Engine() : p_nextTransactId(1), p_nextSequenceId(1) {
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
	
	p_writeAhead.setPath(p_path);
	p_writeAhead.setIdentifier("transact");
	p_writeAhead.loadLog();

	auto closure = new ReplayClosure(this);
	closure->replay();
}

Engine::ReplayClosure::ReplayClosure(Engine *engine)
	: p_engine(engine) { }

void Engine::ReplayClosure::replay() {
	p_engine->p_writeAhead.replay(ASYNC_MEMBER(this, &ReplayClosure::onEntry));
}
void Engine::ReplayClosure::onEntry(Proto::LogEntry &log_entry) {
	if(log_entry.type() == Proto::LogEntry::kTypeSubmit) {
		TransactionId transact_id = log_entry.transaction_id();
		Transaction *transaction = Transaction::allocate();

		for(int i = 0; i < log_entry.mutations_size(); i++) {
			const Proto::LogMutation &log_mutation = log_entry.mutations(i);

			Mutation mutation;
			if(log_mutation.type() == Proto::LogMutation::kTypeInsert) {
				int storage = p_engine->getStorage(log_mutation.storage_name());
				mutation.type = Mutation::kTypeInsert;
				mutation.storageIndex = storage;
				mutation.documentId = log_mutation.document_id();
				mutation.buffer = log_mutation.buffer();
			}else if(log_mutation.type() == Proto::LogMutation::kTypeModify) {
				int storage = p_engine->getStorage(log_mutation.storage_name());
				mutation.type = Mutation::kTypeInsert;
				mutation.storageIndex = storage;
				mutation.documentId = log_mutation.document_id();
				mutation.buffer = log_mutation.buffer();
			}else throw std::logic_error("Illegal log mutation type");

			transaction->mutations.push_back(std::move(mutation));
		}

		p_transactions.insert(std::make_pair(transact_id, transaction));
	}else if(log_entry.type() == Proto::LogEntry::kTypeCommit) {
		TransactionId transact_id = log_entry.transaction_id();
		auto transact_it = p_transactions.find(transact_id);
		if(transact_it == p_transactions.end())
			throw std::runtime_error("Illegal transaction");
		Transaction *transaction = transact_it->second;
		
		for(auto it = p_engine->p_storage.begin(); it != p_engine->p_storage.end(); ++it) {
			StorageDriver *driver = *it;
			if(driver == nullptr)
				continue;
			transaction->refIncrement();
			driver->sequence(log_entry.sequence_id(), transaction->mutations,
					ASYNC_MEMBER(transaction, &Transaction::refDecrement));
		}
		for(auto it = p_engine->p_views.begin(); it != p_engine->p_views.end(); ++it) {
			ViewDriver *driver = *it;
			if(driver == nullptr)
				continue;
			transaction->refIncrement();
			driver->sequence(log_entry.sequence_id(), transaction->mutations,
					ASYNC_MEMBER(transaction, &Transaction::refDecrement));
		}
		
		p_transactions.erase(transact_it);
		transaction->refDecrement();
	}else throw std::logic_error("Illegal log entry type");
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

void Engine::transaction(Async::Callback<void(Error, TransactionId)> callback) {
	TransactionId trid = p_nextTransactId++;

	Transaction *transaction = Transaction::allocate();
	p_openTransactions.insert(std::make_pair(trid, transaction));
	callback(Error(true), trid);
}
void Engine::updateMutation(TransactionId trid, Mutation &mutation,
		Async::Callback<void(Error)> callback) {
	auto transact_it = p_openTransactions.find(trid);
	if(transact_it == p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = transact_it->second;
	
	if(mutation.type == Mutation::kTypeInsert) {
		StorageDriver *driver = p_storage[mutation.storageIndex];
		
		mutation.documentId = driver->allocate();
	}
	transaction->mutations.push_back(std::move(mutation));

	callback(Error(true));
}
void Engine::submit(TransactionId trid,
		Async::Callback<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kTypeSubmit;
	queued.trid = trid;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->increment();
}
void Engine::commit(TransactionId trid,
		Async::Callback<void(Error)> callback) {
	Queued queued;
	queued.type = Queued::kTypeCommit;
	queued.trid = trid;
	queued.callback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->increment();
}

void Engine::process() {
	void (*finish)(void *) = [](void *) { std::cout << "fin" << std::endl; };
	auto op = new ProcessQueueClosure(this,
			Async::Callback<void()>(nullptr, finish));
	op->processLoop();
}

Engine::ProcessQueueClosure::ProcessQueueClosure(Engine *engine,
		Async::Callback<void()> callback)
	: p_engine(engine), p_callback(callback) { }

void Engine::ProcessQueueClosure::processLoop() {
	p_engine->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}

void Engine::ProcessQueueClosure::processItem() {
	if(!p_engine->p_submitQueue.empty()) {
		p_queueItem = p_engine->p_submitQueue.front();
		p_engine->p_submitQueue.pop_front();
		if(p_queueItem.type == Queued::kTypeSubmit) {
			processSubmit();
		}else if(p_queueItem.type == Queued::kTypeCommit) {
			processCommit();
		}else throw std::logic_error("Queued item has illegal type");
	}else{
		osIntf->nextTick(ASYNC_MEMBER(this, &ProcessQueueClosure::processLoop));
	}
}
	
void Engine::ProcessQueueClosure::processSubmit() {
	auto transact_it = p_engine->p_openTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;
	
	p_logEntry.set_type(Proto::LogEntry::kTypeSubmit);
	p_logEntry.set_transaction_id(p_queueItem.trid);

	for(auto it = p_transaction->mutations.begin();
			it != p_transaction->mutations.end(); ++it) {
		Mutation &mutation = *it;

		Proto::LogMutation *log_mutation = p_logEntry.add_mutations();
		if(mutation.type == Mutation::kTypeInsert) {
			StorageDriver *driver = p_engine->p_storage[mutation.storageIndex];
			
			log_mutation->set_type(Proto::LogMutation::kTypeInsert);
			log_mutation->set_storage_name(driver->getIdentifier());
			log_mutation->set_document_id(mutation.documentId);
			log_mutation->set_buffer(mutation.buffer);
		}else if(mutation.type == Mutation::kTypeModify) {
			StorageDriver *driver = p_engine->p_storage[mutation.storageIndex];
			
			log_mutation->set_type(Proto::LogMutation::kTypeModify);
			log_mutation->set_storage_name(driver->getIdentifier());
			log_mutation->set_document_id(mutation.documentId);
			log_mutation->set_buffer(mutation.buffer);
		}else throw std::logic_error("Illegal mutation type");
	}

	p_engine->p_writeAhead.log(p_logEntry,
			ASYNC_MEMBER(this, &ProcessQueueClosure::onSubmitWriteAhead));
}
void Engine::ProcessQueueClosure::onSubmitWriteAhead(Error error) {
	//TODO: handle failure

	p_queueItem.callback(Error(true));

	p_transaction = nullptr;
	p_logEntry.Clear();
	osIntf->nextTick(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}

void Engine::ProcessQueueClosure::processCommit() {
	auto transact_it = p_engine->p_openTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;

	p_sequenceId = p_engine->p_nextSequenceId;
	p_engine->p_nextSequenceId++;
	
	p_logEntry.set_type(Proto::LogEntry::kTypeCommit);
	p_logEntry.set_transaction_id(p_queueItem.trid);
	p_logEntry.set_sequence_id(p_sequenceId);

	p_engine->p_writeAhead.log(p_logEntry,
			ASYNC_MEMBER(this, &ProcessQueueClosure::onCommitWriteAhead));
}
void Engine::ProcessQueueClosure::onCommitWriteAhead(Error error) {
	//TODO: handle failure

	for(auto it = p_engine->p_storage.begin(); it != p_engine->p_storage.end(); ++it) {
		StorageDriver *driver = *it;
		if(driver == nullptr)
			continue;
		p_transaction->refIncrement();
		driver->sequence(p_sequenceId, p_transaction->mutations,
				ASYNC_MEMBER(p_transaction, &Transaction::refDecrement));
	}
	for(auto it = p_engine->p_views.begin(); it != p_engine->p_views.end(); ++it) {
		ViewDriver *driver = *it;
		if(driver == nullptr)
			continue;
		p_transaction->refIncrement();
		driver->sequence(p_sequenceId, p_transaction->mutations,
				ASYNC_MEMBER(p_transaction, &Transaction::refDecrement));
	}
	
	// commit/rollback always frees the transaction
	auto transact_it = p_engine->p_openTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_openTransactions.end())
		throw std::logic_error("Transaction deleted during commit");
	p_engine->p_openTransactions.erase(transact_it);
	p_transaction->refDecrement();
	
	p_queueItem.callback(Error(true));

	p_transaction = nullptr;
	p_logEntry.Clear();
	osIntf->nextTick(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}

void Engine::fetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(Error)> callback) {
	StorageDriver *driver = p_storage[fetch->storageIndex];
	driver->processFetch(fetch, on_data, callback);
}

Error Engine::query(Query *request,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(Error)> callback) {
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

Engine::Transaction *Engine::Transaction::allocate() {
	return new Transaction;
}
void Engine::Transaction::refIncrement() {
	p_refCount++;
}
void Engine::Transaction::refDecrement() {
	assert(p_refCount > 0);
	p_refCount--;
	if(p_refCount == 0)
		delete this;
}

};

