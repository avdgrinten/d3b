
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "Ll/WriteAhead.hpp"

namespace Db {

class Engine {
public:
	Engine();
	
	void createConfig();
	void loadConfig();

	void createStorage(const Proto::StorageConfig &config);
	int getStorage(const std::string &identifier);
	void unlinkStorage(int storage);

	void createView(const Proto::ViewConfig &config);
	int getView(const std::string &view);
	void unlinkView(int view);

	// begin a transaction. returns a transaction id
	void transaction(std::function<void(Error, trid_type)> callback);
	// add a mutation to an existing transaction
	void updateMutation(trid_type trid, Mutation *mutation,
			std::function<void(Error)> callback);
	// submits the transaction.
	// after submit() return success commit() is guaranteed to succeed
	void submit(trid_type trid,
			std::function<void(Error)> callback);
	void commit(trid_type trid,
			std::function<void(Error)> callback);
	void rollback(trid_type trid,
			std::function<void(Error)> callback);

	void fetch(FetchRequest *fetch,
			std::function<void(FetchData &)> on_data,
			std::function<void(Error)> callback);
	
	Error query(Query *request,
			std::function<void(QueryData &)> report,
			std::function<void(Error)> callback);
	
	void process();
	
	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}
	
private:
	/* ------- request and update queue -------- */
	struct Queued {
		enum Type {
			kTypeNone, kTypeSubmit, kTypeCommit, kTypeRollback
		};
		
		Type type;
		trid_type trid;
		std::function<void(Error)> callback;
	};
	
	std::unique_ptr<Linux::EventFd> p_eventFd;
	std::deque<Queued> p_submitQueue;

	void p_processQueue(std::function<void(Error)> callback);
	
	/* --------- active transactions -------- */
	struct Transaction {
		std::vector<Mutation*> mutations;
	};

	trid_type p_nextTransactId;
	std::unordered_map<trid_type, Transaction*> p_openTransactions;

	Ll::WriteAhead p_writeAhead;
	
	std::string p_path;
	std::vector<StorageDriver*> p_storage;
	std::vector<ViewDriver*> p_views;

	StorageDriver *p_setupStorage(const Proto::StorageConfig &config);
	ViewDriver *p_setupView(const Proto::ViewConfig &config);
	
	void p_writeConfig();
};

};

