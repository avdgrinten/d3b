
#include <iostream>
#include <algorithm>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

namespace Db {

StorageRegistry globStorageRegistry;
ViewRegistry globViewRegistry;

Engine::Engine(TaskPool *io_pool) : p_nextTransactId(1), p_currentSequenceId(0),
		p_ioPool(io_pool) {
	p_storage.push_back(nullptr);
	p_views.push_back(nullptr);

	p_eventFd = osIntf->createEventFd();
}

TaskPool *Engine::getIoPool() {
	return p_ioPool;
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

	ReplayClosure closure = ReplayClosure(this);
	closure.replay();
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

SequenceId Engine::currentSequenceId() {
	return p_currentSequenceId;
}

TransactionId Engine::transaction() {
	TransactionId transaction_id = p_nextTransactId++;

	Transaction *transaction = Transaction::allocate();
	p_openTransactions.insert(std::make_pair(transaction_id, transaction));
	return transaction_id;
}
void Engine::updateMutation(TransactionId transaction_id, Mutation &mutation) {
	auto transact_it = p_openTransactions.find(transaction_id);
	if(transact_it == p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = transact_it->second;
	
	if(mutation.type == Mutation::kTypeInsert) {
		StorageDriver *driver = p_storage[mutation.storageIndex];
		mutation.documentId = driver->allocate();
	}

	transaction->mutations.push_back(std::move(mutation));
}
void Engine::updateConstraint(TransactionId transaction_id, Constraint &constraint) {
	auto transact_it = p_openTransactions.find(transaction_id);
	if(transact_it == p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = transact_it->second;
	
	transaction->constraints.push_back(std::move(constraint));
}
void Engine::submit(TransactionId transaction_id,
		Async::Callback<void(SubmitError)> callback) {
	QueueItem queued;
	queued.type = QueueItem::kTypeSubmit;
	queued.trid = transaction_id;
	queued.submitCallback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->increment();
}
void Engine::commit(TransactionId transaction_id,
		Async::Callback<void(SequenceId)> callback) {
	QueueItem queued;
	queued.type = QueueItem::kTypeCommit;
	queued.trid = transaction_id;
	queued.commitCallback = callback;
	p_submitQueue.push_back(queued);
	p_eventFd->increment();
}

bool Engine::compatible(Mutation &mutation, Constraint &constraint) {
	if(constraint.type == Constraint::kTypeDocumentState
			&& constraint.matchSequenceId
			&& mutation.type == Mutation::kTypeModify
			&& mutation.documentId == constraint.documentId)
		return false;
	return true;
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
		if(p_queueItem.type == QueueItem::kTypeSubmit) {
			processSubmit();
		}else if(p_queueItem.type == QueueItem::kTypeCommit) {
			processCommit();
		}else throw std::logic_error("Queued item has illegal type");
	}else{
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::processLoop));
	}
}
	
void Engine::ProcessQueueClosure::processSubmit() {
	auto transact_it = p_engine->p_openTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;

	for(auto other_it = p_engine->p_submittedTransactions.begin();
			other_it != p_engine->p_submittedTransactions.end(); ++other_it) {
		Transaction *other = p_engine->p_openTransactions.at(*other_it);

		// check conflicts: other's mutation <-> tranasction's constraint
		for(auto other_mutation_it = other->mutations.begin();
				other_mutation_it != other->mutations.end(); ++other_mutation_it) {
			for(auto constraint_it = p_transaction->constraints.begin();
					constraint_it != p_transaction->constraints.end(); ++constraint_it) {
				if(!p_engine->compatible(*other_mutation_it, *constraint_it)) {
					submitFailure(kSubmitConstraintConflict);
					return;
				}
			}
		}
		
		// check conflicts: other's constraint <-> transaction's mutation
		for(auto other_constraint_it = other->constraints.begin();
				other_constraint_it != other->constraints.end(); ++ other_constraint_it) {
			for(auto mutation_it = p_transaction->mutations.begin();
					mutation_it != p_transaction->mutations.end(); ++mutation_it) {
				if(!p_engine->compatible(*mutation_it, *other_constraint_it)) {
					submitFailure(kSubmitMutationConflict);
					return;
				}
			}
		}
	}

	p_index = 0;
	submitCheckConstraint();
}
void Engine::ProcessQueueClosure::submitCheckConstraint() {
	if(p_index == p_transaction->constraints.size()) {
		submitLog();
	}else{
		p_checkOkay = true;

		Constraint &constraint = p_transaction->constraints[p_index];
		if(constraint.type == Constraint::kTypeDocumentState) {
			StorageDriver *driver = p_engine->p_storage[constraint.storageIndex];

			p_fetch.documentId = constraint.documentId;
			p_fetch.sequenceId = p_engine->p_currentSequenceId;

			driver->fetch(&p_fetch,
					ASYNC_MEMBER(this, &ProcessQueueClosure::checkStateOnData),
					ASYNC_MEMBER(this, &ProcessQueueClosure::checkStateOnFetch));
		}else throw std::logic_error("Illegal constraint type");
	}
}
void Engine::ProcessQueueClosure::checkStateOnData(FetchData &data) {
	Constraint &constraint = p_transaction->constraints[p_index];
//	if(data.sequenceId != constraint.sequenceId)
//		p_checkOkay = false;
}
void Engine::ProcessQueueClosure::checkStateOnFetch(FetchError error) {
	Constraint &constraint = p_transaction->constraints[p_index];
	if(error == kFetchSuccess) {
		// everything is okay
	}else if(error == kFetchDocumentNotFound) {
		if(constraint.mustExist)
			p_checkOkay = false;
	}else throw std::logic_error("Unexpected error during fetch");

	if(!p_checkOkay) {
		submitFailure(kSubmitConstraintViolation);
		return;
	}
	
	p_index++;
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::submitCheckConstraint));
}
void Engine::ProcessQueueClosure::submitLog() {
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

	p_engine->p_submittedTransactions.push_back(p_queueItem.trid);

	p_queueItem.submitCallback(kSubmitSuccess);

	p_transaction = nullptr;
	p_logEntry.Clear();
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}
void Engine::ProcessQueueClosure::submitFailure(SubmitError error) {
	auto iterator = p_engine->p_openTransactions.find(p_queueItem.trid);
	assert(iterator != p_engine->p_openTransactions.end());
	p_engine->p_openTransactions.erase(iterator);

	p_transaction->refDecrement();

	p_queueItem.submitCallback(error);
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}

void Engine::ProcessQueueClosure::processCommit() {
	auto transact_it = p_engine->p_openTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_openTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;

	p_engine->p_currentSequenceId++;
	p_sequenceId = p_engine->p_currentSequenceId;
	
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
	auto open_iterator = p_engine->p_openTransactions.find(p_queueItem.trid);
	assert(open_iterator != p_engine->p_openTransactions.end());
	p_engine->p_openTransactions.erase(open_iterator);

	auto submit_iterator = std::find(p_engine->p_submittedTransactions.begin(),
			p_engine->p_submittedTransactions.end(), p_queueItem.trid);
	assert(submit_iterator != p_engine->p_submittedTransactions.end());
	p_engine->p_submittedTransactions.erase(submit_iterator);

	p_transaction->refDecrement();

	p_queueItem.commitCallback(p_sequenceId);

	p_transaction = nullptr;
	p_logEntry.Clear();
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::processItem));
}

void Engine::fetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(FetchError)> callback) {
	StorageDriver *driver = p_storage[fetch->storageIndex];
	driver->fetch(fetch, on_data, callback);
}

void Engine::query(QueryRequest *request,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(QueryError)> callback) {
	ViewDriver *driver = p_views[request->viewIndex];
	driver->query(request, on_data, callback);
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

