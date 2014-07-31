
#include "Async.hpp"

namespace Db {

class Engine;

struct QueryRequest {
	int viewIndex;
	SequenceId sequenceId;

	bool useFromKey;
	bool useToKey;
	
	std::string fromKey;
	std::string toKey;
	int limit;

	QueryRequest() : useFromKey(false), useToKey(false), limit(-1) { }
};

struct QueryData {
	std::vector<std::string> items;
};

enum QueryError {
	kQueryNone = 0,
	kQuerySuccess = 1
};

class ViewDriver {
public:
	class Factory {
	public:
		inline Factory(const std::string &identifier)
			: p_identifier(identifier) {
		}
		
		virtual ViewDriver *newDriver(Engine *engine) = 0;
		
		inline std::string getIdentifier() {
			return p_identifier;
		}
	protected:
		std::string p_identifier;
	};
	
	ViewDriver(Engine *engine)
			: p_engine(engine) {
	}

	virtual void createView() = 0;
	virtual void loadView() = 0;

	virtual Proto::ViewConfig writeConfig() = 0;
	virtual void readConfig(const Proto::ViewConfig &config) = 0;

	virtual void sequence(SequenceId sequence_id,
			std::vector<Mutation> &mutations,
			Async::Callback<void()> callback) = 0;

	virtual void query(QueryRequest *request,
			Async::Callback<void(QueryData &)> report,
			Async::Callback<void(QueryError)> callback) = 0;

	inline Engine *getEngine() {
		return p_engine;
	}

	inline void setIdentifier(const std::string &identifier) {
		p_identifier = identifier;
	}
	inline std::string getIdentifier() {
		return p_identifier;
	}
	
	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}

protected:
	Engine *p_engine;
	std::string p_identifier;
	std::string p_path;
};

class ViewRegistry {
public:
	void addDriver(ViewDriver::Factory *factory) {
		p_drivers.push_back(factory);
	}

	inline ViewDriver::Factory *getDriver
			(const std::string &identifier) {
		for(int i = 0; i < p_drivers.size(); ++i)
			if(p_drivers[i]->getIdentifier() == identifier)
				return p_drivers[i];
		return nullptr;
	}
	
private:
	std::vector<ViewDriver::Factory*> p_drivers;
};

class QueuedViewDriver : public ViewDriver {
public:
	QueuedViewDriver(Engine *engine);

	virtual void sequence(SequenceId sequence_id,
			std::vector<Mutation> &mutations,
			Async::Callback<void()> callback);

	virtual void query(QueryRequest *query,
			Async::Callback<void(QueryData &)> on_data,
			Async::Callback<void(QueryError)> callback);

protected:
	void processQueue();

	void finishRequest();

	virtual void processInsert(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback) = 0;
	virtual void processModify(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback) = 0;

	virtual void processQuery(QueryRequest *query,
			Async::Callback<void(QueryData &)> on_data,
			Async::Callback<void(QueryError)> callback) = 0;

private:
	struct SequenceQueueItem {
		SequenceId sequenceId;
		std::vector<Mutation> *mutations;
		Async::Callback<void()> callback;
	};
	struct QueryQueueItem {
		QueryRequest *query;
		Async::Callback<void(QueryData &)> onData;
		Async::Callback<void(QueryError)> callback;
	};
	
	SequenceId p_currentSequenceId;

	std::queue<SequenceQueueItem> p_sequenceQueue;
	std::queue<QueryQueueItem> p_queryQueue;
	std::unique_ptr<Linux::EventFd> p_eventFd;
	std::mutex p_mutex;
	int p_activeRequests;
	bool p_requestPhase;
	
	class ProcessClosure {
	public:
		ProcessClosure(QueuedViewDriver *view);

		void process();
	
	private:
		void endRequestPhase();
		void sequencePhase();
		void processSequence();
		void onSequenceItem(Error error);

		QueuedViewDriver *p_view;

		SequenceQueueItem p_sequenceItem;
		int p_index;
	};
};

extern ViewRegistry globViewRegistry;

};

