
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "Ll/PageCache.hpp"
#include "Ll/WriteAhead.hpp"

namespace Db {

enum SubmitError {
	kSubmitNone = 0,
	kSubmitSuccess = 1,
	// constraint violated under current database state
	kSubmitConstraintViolation = 2,
	// constraint of this transaction conflicts with mutation of other transaction
	kSubmitConstraintConflict = 3,
	// mutation of this transaction conflicts with constraint of other transaction
	kSubmitMutationConflict = 4
};

class Engine {
public:
	Engine();
	
	void createConfig();
	void loadConfig();

	void createStorage(const std::string &driver,
			const std::string &identifier,
			const Proto::StorageConfig &config);
	int getStorage(const std::string &identifier);
	void unlinkStorage(int storage);

	void createView(const std::string &driver,
			const std::string &identifier,
			const Proto::ViewConfig &config);
	int getView(const std::string &view);
	void unlinkView(int view);

	SequenceId currentSequenceId();

	// begin a transaction. returns a transaction id
	TransactionId transaction();
	// add a mutation to an existing transaction
	void updateMutation(TransactionId trid, Mutation &mutation);
	// add a constraint to an existing transaction
	void updateConstraint(TransactionId trid, Constraint &constraint);
	// submits the transaction.
	// after submit() return success commit() is guaranteed to succeed
	void submit(TransactionId trid,
			Async::Callback<void(SubmitError)> callback);
	void submitCommit(TransactionId trid,
			Async::Callback<void(std::pair<SubmitError, SequenceId>)> callback);
	void commit(TransactionId trid,
			Async::Callback<void(SequenceId)> callback);
	void rollback(TransactionId trid,
			Async::Callback<void()> callback);

	void fetch(FetchRequest *fetch,
			Async::Callback<void(FetchData &)> on_data,
			Async::Callback<void(FetchError)> callback);

	void query(QueryRequest *request,
			Async::Callback<void(QueryData &)> report,
			Async::Callback<void(QueryError)> callback);
	
	CacheHost *getCacheHost();
	TaskPool *getProcessPool();
	TaskPool *getIoPool();

	void process();

	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}

private:
	struct QueueItem {
		enum Type {
			kTypeNone, kTypeSubmit, kTypeSubmitCommit, kTypeCommit, kTypeRollback
		};
		
		Type type;
		TransactionId trid;
		Async::Callback<void(Error)> callback;
		Async::Callback<void(SubmitError)> submitCallback;
		Async::Callback<void(std::pair<SubmitError, SequenceId>)> submitCommitCallback;
		Async::Callback<void(SequenceId)> commitCallback;
		Async::Callback<void()> rollbackCallback;
	};

	class Transaction {
	public:
		enum State {
			kStateNone,
			// transaction can still be modified
			kStateOpen,
			// transaction has been queued for submit
			kStateInSubmit,
			// transaction has been successfully submitted
			kStateSubmitted,
			// transaction has been queued for commit or rollback
			kStateInCommitOrRollback
		};

		static Transaction *allocate();
		
		void refIncrement();
		void refDecrement();

		std::vector<Mutation> mutations;
		std::vector<Constraint> constraints;
		
		State state;
	private:
		Transaction() : p_refCount(1) { }

		int p_refCount;
	};

	StorageDriver *setupStorage(const std::string &driver,
			const std::string &identifier);
	ViewDriver *setupView(const std::string &driver,
			const std::string &identifier);
	
	void writeConfig();

	bool compatible(Mutation &mutation, Constraint &constraint);

	CacheHost p_cacheHost;
	TaskPool p_processPool;
	TaskPool p_ioPool;
	
	std::unique_ptr<Linux::EventFd> p_eventFd;
	std::deque<QueueItem> p_submitQueue;

	TransactionId p_nextTransactId;
	SequenceId p_currentSequenceId;
	std::unordered_map<TransactionId, Transaction *> p_activeTransactions;
	std::vector<TransactionId> p_submittedTransactions;
	std::mutex p_mutex;

	Ll::WriteAhead p_writeAhead;
	
	std::string p_path;
	std::vector<StorageDriver*> p_storages;
	std::vector<ViewDriver*> p_views;

	class ProcessQueueClosure {
	public:
		ProcessQueueClosure(Engine *engine, Async::Callback<void()> callback);

		void process();

	private:
		void processSubmit();
		void submitCheckConstraint();
		void checkStateOnData(FetchData &data);
		void checkStateOnFetch(FetchError error);
		void submitComplete();
		void submitFailure(SubmitError error);
		void processCommit();
		void commitComplete();
		void writeAhead();
		void afterWriteAhead(Error error);
		void processRollback();

		Engine *p_engine;
		
		QueueItem p_queueItem;
		Transaction *p_transaction;
		SequenceId p_sequenceId;
		Proto::LogEntry p_logEntry;
		int p_index;
		bool p_checkOkay;

		FetchRequest p_fetch;

		Async::Callback<void()> p_callback;
	};

	class ReplayClosure {
	public:
		ReplayClosure(Engine *engine);
		
		void replay();
		Engine *getEngine();
	
	protected:
		// called for each transaction, even if it was rolled back
		virtual void onTransaction(TransactionId id,
				Transaction *transaction) = 0;
		// called for transactions that have been submitted and comitted
		virtual void onCommit(Transaction *transaction,
				SequenceId sequence_id) = 0;
		// called for transaction that have been submitted but not committed or rolled back
		virtual void onSubmitted(TransactionId id, Transaction *transaction) = 0;
		
	private:
		void onEntry(Proto::LogEntry &entry);

		Engine *p_engine;

		std::unordered_map<TransactionId, Transaction *> p_transactions;
	};
	
	class ReplayMetaClosure : public ReplayClosure {
	public:
		ReplayMetaClosure(Engine *engine);

	protected:
		virtual void onTransaction(TransactionId id,
				Transaction *transaction);
		virtual void onCommit(Transaction *transaction,
				SequenceId sequence_id);
		virtual void onSubmitted(TransactionId id, Transaction *transaction);
	};

	class ReplayDataClosure : public ReplayClosure {
	public:
		ReplayDataClosure(Engine *engine,
				Sequenceable *sequenceable);

	protected:
		virtual void onTransaction(TransactionId id,
				Transaction *transaction);
		virtual void onCommit(Transaction *transaction,
				SequenceId sequence_id);
		virtual void onSubmitted(TransactionId id, Transaction *transaction);
	
	private:
		Sequenceable *p_sequenceable;
	};
};

};

