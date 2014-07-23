
#include "Async.hpp"

#include "proto/Config.pb.h"

namespace Db {

class Engine;

struct FetchRequest {
	int storageIndex;
	DocumentId documentId;
	SequenceId sequenceId;
};

struct FetchData {
	DocumentId documentId;
	SequenceId sequenceId;
	std::string buffer;
};

class StorageDriver {
public:
	class Factory {
	public:
		inline Factory(const std::string &identifier)
			: p_identifier(identifier) {
		}
		
		virtual StorageDriver *newDriver(Engine *engine) = 0;
		
		inline std::string getIdentifier() {
			return p_identifier;
		}
	protected:
		std::string p_identifier;
	};

	StorageDriver(Engine *engine) : p_engine(engine) { };

	virtual void createStorage() = 0;
	virtual void loadStorage() = 0;

	virtual Proto::StorageConfig writeConfig() = 0;
	virtual void readConfig(const Proto::StorageConfig &config) = 0;
	
	// returns an unused and unique document id.
	// must never return the same value more than once
	virtual DocumentId allocate() = 0;

	/* commits a transaction to the storage */
	virtual void sequence(SequenceId sequence_id,
			std::vector<Mutation> &mutations,
			Async::Callback<void()> callback) = 0;

	/* processes a query */
	virtual void processFetch(FetchRequest *fetch,
			Async::Callback<void(FetchData &)> on_data,
			Async::Callback<void(Error)> callback) = 0;

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

class StorageRegistry {
public:
	void addDriver(StorageDriver::Factory *factory) {
		p_drivers.push_back(factory);
	}

	inline StorageDriver::Factory *getDriver
			(const std::string &identifier) {
		for(int i = 0; i < p_drivers.size(); ++i)
			if(p_drivers[i]->getIdentifier() == identifier)
				return p_drivers[i];
		return nullptr;
	}
	
private:
	std::vector<StorageDriver::Factory*> p_drivers;
};

extern StorageRegistry globStorageRegistry;

};

