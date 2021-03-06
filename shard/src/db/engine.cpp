
#include <iostream>
#include <algorithm>

#include "async.hpp"
#include "os/linux.hpp"
#include "ll/tasks.hpp"

#include "db/types.hpp"
#include "db/storage-driver.hpp"
#include "db/view-driver.hpp"
#include "db/engine.hpp"

namespace Db {

StorageRegistry globStorageRegistry;
ViewRegistry globViewRegistry;

Engine::Engine() : p_nextTransactId(1), p_currentSequenceId(0) {
	p_storages.push_back(nullptr);
	p_views.push_back(nullptr);

	p_eventFd = osIntf->createEventFd();

	p_cacheHost.setLimit(16 * 4096);
}

CacheHost *Engine::getCacheHost() {
	return &p_cacheHost;
}
TaskPool *Engine::getProcessPool() {
	return &p_processPool;
}
TaskPool *Engine::getIoPool() {
	return &p_ioPool;
}

void Engine::createConfig() {
	if(osIntf->fileExists(p_path + "/config"))
		throw std::runtime_error("Config file exists already!");
	osIntf->mkDir(p_path + "/storages");
	osIntf->mkDir(p_path + "/views");
	osIntf->mkDir(p_path + "/extern");
	
	p_writeAhead.setPath(p_path);
	p_writeAhead.setIdentifier("transact");
	p_writeAhead.createLog();
	
	writeConfig();
}

void Engine::loadConfig() {
	Proto::Config config;
	config.ParseFromString(OS::readFileSync(p_path + "/config"));
	
	for(int i = 0; i < config.storages_size(); i++) {
		const std::string identifier = config.storages(i);

		auto desc_path = p_path + "/storages/" + identifier + "/descriptor";
		Proto::StorageDescriptor descriptor;
		descriptor.ParseFromString(OS::readFileSync(desc_path));

		StorageDriver *instance = setupStorage(descriptor.driver(), identifier);
		instance->loadStorage();
	}
	
	for(int i = 0; i < config.views_size(); i++) {
		const std::string identifier = config.views(i);

		auto desc_path = p_path + "/views/" + identifier + "/descriptor";
		Proto::ViewDescriptor descriptor;
		descriptor.ParseFromString(OS::readFileSync(desc_path));

		ViewDriver *instance = setupView(descriptor.driver(), identifier);
		instance->loadView();
	}
	
	p_writeAhead.setPath(p_path);
	p_writeAhead.setIdentifier("transact");
	p_writeAhead.loadLog();

	ReplayMetaClosure meta_closure = ReplayMetaClosure(this);
	meta_closure.replay();
	
	for(auto it = p_storages.begin(); it != p_storages.end(); ++it) {
		StorageDriver *driver = *it;
		if(driver == nullptr)
			continue;
		ReplayDataClosure data_closure = ReplayDataClosure(this, driver);
		data_closure.replay();
	}
	for(auto it = p_views.begin(); it != p_views.end(); ++it) {
		ViewDriver *driver = *it;
		if(driver == nullptr)
			continue;
		ReplayDataClosure data_closure = ReplayDataClosure(this, driver);
		data_closure.replay();
	}
}

void Engine::createStorage(const std::string &driver,
		const std::string &identifier, const Proto::StorageConfig &config) {
	if(getStorage(identifier) != -1)
		throw std::runtime_error("Storage exists already!");
	osIntf->mkDir(p_path + "/storages/" + identifier);
	StorageDriver *instance = setupStorage(driver, identifier);
	instance->createStorage();
	
	auto desc_path = p_path + "/storages/" + identifier + "/descriptor";
	Proto::StorageDescriptor descriptor;
	descriptor.set_driver(driver);
	OS::writeFileSync(desc_path, descriptor.SerializeAsString());

	writeConfig();
}
int Engine::getStorage(const std::string &identifier) {
	for(int i = 1; i < p_storages.size(); i++) {
		if(p_storages[i] == nullptr)
			continue;
		if(p_storages[i]->getIdentifier() == identifier)
			return i;
	}
	return -1;
}
void Engine::unlinkStorage(int storage) {
	if(storage == -1)
		throw std::runtime_error("Illegal storage specified");
	StorageDriver *driver = p_storages[storage];
	if(driver == nullptr)
		throw std::runtime_error("Requested storage does not exist");
	p_storages[storage] = nullptr;
	writeConfig();
}

void Engine::createView(const std::string &driver,
		const std::string &identifier, const Proto::ViewConfig &config) {
	if(getView(identifier) != -1)
		throw std::runtime_error("View exists already!");
	osIntf->mkDir(p_path + "/views/" + identifier);
	ViewDriver *instance = setupView(driver, identifier);
	instance->createView(config);
	
	auto desc_path = p_path + "/views/" + identifier + "/descriptor";
	Proto::ViewDescriptor descriptor;
	descriptor.set_driver(driver);
	OS::writeFileSync(desc_path, descriptor.SerializeAsString());

	writeConfig();
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
	writeConfig();
}

SequenceId Engine::currentSequenceId() {
	std::lock_guard<std::mutex> lock(p_mutex);

	return p_currentSequenceId;
}

TransactionId Engine::transaction() {
	std::lock_guard<std::mutex> lock(p_mutex);

	TransactionId transaction_id = p_nextTransactId++;

	Transaction *transaction = Transaction::allocate();
	transaction->state = Transaction::kStateOpen;
	p_activeTransactions.insert(std::make_pair(transaction_id, transaction));
	return transaction_id;
}
void Engine::updateMutation(TransactionId transaction_id, Mutation &mutation) {
	std::lock_guard<std::mutex> lock(p_mutex);

	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateOpen);

	if(mutation.type == Mutation::kTypeInsert) {
		StorageDriver *driver = p_storages[mutation.storageIndex];
		mutation.documentId = driver->allocate();
	}

	transaction->mutations.push_back(std::move(mutation));
}
void Engine::updateConstraint(TransactionId transaction_id, Constraint &constraint) {
	std::lock_guard<std::mutex> lock(p_mutex);
	
	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateOpen);
	
	transaction->constraints.push_back(std::move(constraint));
}
void Engine::submit(TransactionId transaction_id,
		Async::Callback<void(SubmitError)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateOpen);
	transaction->state = Transaction::kStateInSubmit;

	QueueItem queued;
	queued.type = QueueItem::kTypeSubmit;
	queued.trid = transaction_id;
	queued.submitCallback = callback;
	p_submitQueue.push_back(queued);

	lock.unlock();
	p_eventFd->increment();
}
void Engine::submitCommit(TransactionId transaction_id,
		Async::Callback<void(std::pair<SubmitError, SequenceId>)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateOpen);
	transaction->state = Transaction::kStateInSubmit;

	QueueItem queued;
	queued.type = QueueItem::kTypeSubmitCommit;
	queued.trid = transaction_id;
	queued.submitCommitCallback = callback;
	p_submitQueue.push_back(queued);

	lock.unlock();
	p_eventFd->increment();
}
void Engine::commit(TransactionId transaction_id,
		Async::Callback<void(SequenceId)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateSubmitted);
	transaction->state = Transaction::kStateInCommitOrRollback;

	QueueItem queued;
	queued.type = QueueItem::kTypeCommit;
	queued.trid = transaction_id;
	queued.commitCallback = callback;
	p_submitQueue.push_back(queued);

	lock.unlock();
	p_eventFd->increment();
}
void Engine::rollback(TransactionId transaction_id,
		Async::Callback<void()> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);
	
	auto iterator = p_activeTransactions.find(transaction_id);
	if(iterator == p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");	
	Transaction *transaction = iterator->second;
	assert(transaction->state == Transaction::kStateOpen
			|| transaction->state == Transaction::kStateSubmitted);
	transaction->state = Transaction::kStateInCommitOrRollback;

	QueueItem queued;
	queued.type = QueueItem::kTypeRollback;
	queued.trid = transaction_id;
	queued.rollbackCallback = callback;
	p_submitQueue.push_back(queued);

	lock.unlock();
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
	op->process();
}

void Engine::fetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(FetchError)> callback) {
	StorageDriver *driver = p_storages[fetch->storageIndex];
	driver->fetch(fetch, on_data, callback);
}

void Engine::query(QueryRequest *request,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(QueryError)> callback) {
	ViewDriver *driver = p_views[request->viewIndex];
	driver->query(request, on_data, callback);
}

StorageDriver *Engine::setupStorage(const std::string &driver,
		const std::string &identifier) {
	StorageDriver::Factory *factory
			= globStorageRegistry.getDriver(driver);
	if(factory == nullptr)
		throw std::runtime_error(std::string("Storage driver '")
			+ driver + std::string("' not available"));
	std::cout << "Setting up storage '" << identifier << "'" << std::endl;

	StorageDriver *instance = factory->newDriver(this);
	instance->setIdentifier(identifier);
	instance->setPath(p_path + "/storages/" + identifier);
	p_storages.push_back(instance);
	return instance;
}

ViewDriver *Engine::setupView(const std::string &driver,
		const std::string &identifier) {
	ViewDriver::Factory *factory
			= globViewRegistry.getDriver(driver);
	if(factory == nullptr)
		throw std::runtime_error(std::string("View driver '")
			+ driver + std::string("' not available"));
	std::cout << "Setting up view '" << identifier << "'" << std::endl;

	ViewDriver *instance = factory->newDriver(this);
	instance->setIdentifier(identifier);
	instance->setPath(p_path + "/views/" + identifier);
	p_views.push_back(instance);
	return instance;
}

void Engine::writeConfig() {
	Proto::Config config;
	
	for(int i = 1; i < p_storages.size(); i++) {
		if(p_storages[i] == nullptr)
			continue;
		config.add_storages(p_storages[i]->getIdentifier());
	}
	for(int i = 1; i < p_views.size(); i++) {
		if(p_views[i] == nullptr)
			continue;
		config.add_views(p_views[i]->getIdentifier());
	}

	OS::writeFileSync(p_path + "/config", config.SerializeAsString());
}

// --------------------------------------------------------
// Engine::Transaction
// --------------------------------------------------------

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

// --------------------------------------------------------
// Engine::ReplayClosure
// --------------------------------------------------------

Engine::ReplayClosure::ReplayClosure(Engine *engine)
	: p_engine(engine) { }

void Engine::ReplayClosure::replay() {
	p_engine->p_writeAhead.replay(ASYNC_MEMBER(this, &ReplayClosure::onEntry));

	for(auto it = p_transactions.begin(); it != p_transactions.end(); ++it)
		onSubmitted(it->first, it->second);
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
		
		onTransaction(transact_id, transaction);

		p_transactions.insert(std::make_pair(transact_id, transaction));
	}else if(log_entry.type() == Proto::LogEntry::kTypeSubmitCommit) {
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
		
		onTransaction(transact_id, transaction);
		onCommit(transaction, log_entry.sequence_id());
		
		transaction->refDecrement();
	}else if(log_entry.type() == Proto::LogEntry::kTypeCommit) {
		TransactionId transact_id = log_entry.transaction_id();
		auto transact_it = p_transactions.find(transact_id);
		if(transact_it == p_transactions.end())
			throw std::runtime_error("Illegal transaction");
		Transaction *transaction = transact_it->second;
		
		onCommit(transaction, log_entry.sequence_id());

		p_transactions.erase(transact_it);
		transaction->refDecrement();
	}else throw std::logic_error("Illegal log entry type");
}

Engine *Engine::ReplayClosure::getEngine() {
	return p_engine;
}

// --------------------------------------------------------
// Engine::ReplayMetaClosure
// --------------------------------------------------------

Engine::ReplayMetaClosure::ReplayMetaClosure(Engine *engine)
		: ReplayClosure(engine) { }

void Engine::ReplayMetaClosure::onTransaction(TransactionId id,
		Transaction *transaction) {
	Engine *engine = getEngine();
	if(engine->p_nextTransactId < id + 1)
		engine->p_nextTransactId = id + 1;
}

void Engine::ReplayMetaClosure::onCommit(Transaction *transaction,
		SequenceId sequence_id) {
	Engine *engine = getEngine();
	if(engine->p_currentSequenceId < sequence_id)
		engine->p_currentSequenceId = sequence_id;
}

void Engine::ReplayMetaClosure::onSubmitted(TransactionId id,
		Transaction *transaction) {
	Engine *engine = getEngine();

	transaction->refIncrement();
	transaction->state = Transaction::kStateSubmitted;
	engine->p_activeTransactions.insert(std::make_pair(id, transaction));
	engine->p_submittedTransactions.push_back(id);
}

// --------------------------------------------------------
// Engine::ReplayDataClosure
// --------------------------------------------------------

Engine::ReplayDataClosure::ReplayDataClosure(Engine *engine, Sequenceable *sequenceable)
		: ReplayClosure(engine), p_sequenceable(sequenceable) { }

void Engine::ReplayDataClosure::onTransaction(TransactionId id,
		Transaction *transaction) {
	for(auto it = transaction->mutations.begin();
			it != transaction->mutations.end(); ++it)
		p_sequenceable->reinspect(*it);
}

void Engine::ReplayDataClosure::onCommit(Transaction *transaction,
		SequenceId sequence_id) {
	Engine *engine = getEngine();

	transaction->refIncrement();
	p_sequenceable->sequence(sequence_id, transaction->mutations,
			ASYNC_MEMBER(transaction, &Transaction::refDecrement));
}

void Engine::ReplayDataClosure::onSubmitted(TransactionId id,
		Transaction *transaction) {
	// ignore non-commited transactions
}

// --------------------------------------------------------
// Engine::ProcessQueueClosure
// --------------------------------------------------------

Engine::ProcessQueueClosure::ProcessQueueClosure(Engine *engine,
		Async::Callback<void()> callback)
	: p_engine(engine), p_callback(callback) { }

void Engine::ProcessQueueClosure::process() {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);

	if(!p_engine->p_submitQueue.empty()) {
		p_queueItem = p_engine->p_submitQueue.front();
		p_engine->p_submitQueue.pop_front();
		
		lock.unlock();
		if(p_queueItem.type == QueueItem::kTypeSubmit
				|| p_queueItem.type == QueueItem::kTypeSubmitCommit) {
			processSubmit();
		}else if(p_queueItem.type == QueueItem::kTypeCommit) {
			processCommit();
		}else if(p_queueItem.type == QueueItem::kTypeRollback) {
			processRollback();
		}else throw std::logic_error("Queued item has illegal type");
	}else{
		lock.unlock();
		p_engine->p_eventFd->wait(ASYNC_MEMBER(this, &ProcessQueueClosure::process));
	}
}
	
void Engine::ProcessQueueClosure::processSubmit() {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);

	auto transact_it = p_engine->p_activeTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;

	lock.unlock();

	for(auto other_it = p_engine->p_submittedTransactions.begin();
			other_it != p_engine->p_submittedTransactions.end(); ++other_it) {
		Transaction *other = p_engine->p_activeTransactions.at(*other_it);

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
		// NOTE: this is one of two places where the sequence id is advanced
		std::unique_lock<std::mutex> lock(p_engine->p_mutex);
		p_engine->p_currentSequenceId++;
		p_sequenceId = p_engine->p_currentSequenceId;
		lock.unlock();

		writeAhead();
	}else{
		p_checkOkay = true;

		Constraint &constraint = p_transaction->constraints[p_index];
		if(constraint.type == Constraint::kTypeDocumentState) {
			StorageDriver *driver = p_engine->p_storages[constraint.storageIndex];

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
//FIXME:
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
void Engine::ProcessQueueClosure::submitComplete() {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);
	p_engine->p_submittedTransactions.push_back(p_queueItem.trid);

	if(p_queueItem.type == QueueItem::kTypeSubmit) {
		p_transaction->state = Transaction::kStateSubmitted;
		lock.unlock();

		p_queueItem.submitCallback(kSubmitSuccess);
		LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::process));
	}else if(p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		lock.unlock();
		commitComplete();
	}else throw std::logic_error("Illegal queue item");
}
void Engine::ProcessQueueClosure::submitFailure(SubmitError error) {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);

	auto iterator = p_engine->p_activeTransactions.find(p_queueItem.trid);
	assert(iterator != p_engine->p_activeTransactions.end());
	p_engine->p_activeTransactions.erase(iterator);

	p_transaction->refDecrement();
	
	if(p_queueItem.type == QueueItem::kTypeSubmit) {
		lock.unlock();
		p_queueItem.submitCallback(error);
	}else if(p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		lock.unlock();
		p_queueItem.submitCommitCallback(std::make_pair(error, -1));
	}else throw std::logic_error("Illegal queue item");
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::process));
}

// NOTE: this function is NOT called when kTypeSubmitCommit is handled
void Engine::ProcessQueueClosure::processCommit() {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);

	auto transact_it = p_engine->p_activeTransactions.find(p_queueItem.trid);
	if(transact_it == p_engine->p_activeTransactions.end())
		throw std::runtime_error("Illegal transaction");
	p_transaction = transact_it->second;
	
	// NOTE: this is one of two places where the sequence id is advanced
	p_engine->p_currentSequenceId++;
	p_sequenceId = p_engine->p_currentSequenceId;

	lock.unlock();
	
	writeAhead();
}
// NOTE: this function is called when kTypeSubmitCommit is handled
void Engine::ProcessQueueClosure::commitComplete() {
	for(auto it = p_engine->p_storages.begin(); it != p_engine->p_storages.end(); ++it) {
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

	std::unique_lock<std::mutex> lock(p_engine->p_mutex);
	
	// commit always frees the transaction
	auto open_iterator = p_engine->p_activeTransactions.find(p_queueItem.trid);
	assert(open_iterator != p_engine->p_activeTransactions.end());
	p_engine->p_activeTransactions.erase(open_iterator);

	auto submit_iterator = std::find(p_engine->p_submittedTransactions.begin(),
			p_engine->p_submittedTransactions.end(), p_queueItem.trid);
	assert(submit_iterator != p_engine->p_submittedTransactions.end());
	p_engine->p_submittedTransactions.erase(submit_iterator);

	p_transaction->refDecrement();

	if(p_queueItem.type == QueueItem::kTypeCommit) {
		lock.unlock();
		p_queueItem.commitCallback(p_sequenceId);
	}else if(p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		lock.unlock();
		p_queueItem.submitCommitCallback(std::make_pair(kSubmitSuccess, p_sequenceId));
	}else throw std::logic_error("Illegal queue item");
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::process));
}
void Engine::ProcessQueueClosure::processRollback() {
	std::unique_lock<std::mutex> lock(p_engine->p_mutex);
	
	auto open_iterator = p_engine->p_activeTransactions.find(p_queueItem.trid);
	assert(open_iterator != p_engine->p_activeTransactions.end());
	p_transaction = open_iterator->second;

	p_engine->p_activeTransactions.erase(open_iterator);

	if(p_transaction->state == Transaction::kStateSubmitted) {
		auto submit_iterator = std::find(p_engine->p_submittedTransactions.begin(),
				p_engine->p_submittedTransactions.end(), p_queueItem.trid);
		assert(submit_iterator != p_engine->p_submittedTransactions.end());
		p_engine->p_submittedTransactions.erase(submit_iterator);
	}

	p_transaction->refDecrement();
	
	lock.unlock();
	p_queueItem.rollbackCallback();
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &ProcessQueueClosure::process));
}

void Engine::ProcessQueueClosure::writeAhead() {
	if(p_queueItem.type == QueueItem::kTypeSubmit) {
		p_logEntry.set_type(Proto::LogEntry::kTypeSubmit);
		p_logEntry.set_transaction_id(p_queueItem.trid);
	}else if(p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		p_logEntry.set_type(Proto::LogEntry::kTypeSubmitCommit);
		p_logEntry.set_transaction_id(p_queueItem.trid);
		p_logEntry.set_sequence_id(p_engine->p_currentSequenceId);
	}else if(p_queueItem.type == QueueItem::kTypeCommit) {
		p_logEntry.set_type(Proto::LogEntry::kTypeCommit);
		p_logEntry.set_transaction_id(p_queueItem.trid);
		p_logEntry.set_sequence_id(p_engine->p_currentSequenceId);
	}else throw std::logic_error("Illegal queue item");

	if(p_queueItem.type == QueueItem::kTypeSubmit
			|| p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		for(auto it = p_transaction->mutations.begin();
				it != p_transaction->mutations.end(); ++it) {
			Mutation &mutation = *it;

			Proto::LogMutation *log_mutation = p_logEntry.add_mutations();
			if(mutation.type == Mutation::kTypeInsert) {
				StorageDriver *driver = p_engine->p_storages[mutation.storageIndex];
				
				log_mutation->set_type(Proto::LogMutation::kTypeInsert);
				log_mutation->set_storage_name(driver->getIdentifier());
				log_mutation->set_document_id(mutation.documentId);
				log_mutation->set_buffer(mutation.buffer);
			}else if(mutation.type == Mutation::kTypeModify) {
				StorageDriver *driver = p_engine->p_storages[mutation.storageIndex];
				
				log_mutation->set_type(Proto::LogMutation::kTypeModify);
				log_mutation->set_storage_name(driver->getIdentifier());
				log_mutation->set_document_id(mutation.documentId);
				log_mutation->set_buffer(mutation.buffer);
			}else throw std::logic_error("Illegal mutation type");
		}
	}

	p_engine->p_writeAhead.log(p_logEntry,
			ASYNC_MEMBER(this, &ProcessQueueClosure::afterWriteAhead));
}
void Engine::ProcessQueueClosure::afterWriteAhead(Error error) {
	//TODO: handle failure
	
	p_logEntry.Clear();

	if(p_queueItem.type == QueueItem::kTypeSubmit
			|| p_queueItem.type == QueueItem::kTypeSubmitCommit) {
		submitComplete();
	}else if(p_queueItem.type == QueueItem::kTypeCommit) {
		commitComplete();
	}else throw std::logic_error("Illegal queue item");
}

};

