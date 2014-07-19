
#include "proto/Config.pb.h"
#include "proto/Request.pb.h"

namespace Db {

class Engine;

struct FetchRequest {
	int storageIndex;
	id_type documentId;
};

struct FetchData {
	id_type documentId;
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

	/* checks whether a mutation is accepted. fills in
		additional details like the allocated id for insert
		requests */
	virtual void updateAccept(Mutation *mutation,
			std::function<void(Error)> callback) = 0;
	/* validates a mutation against the current state.
		returning success means that it is okay to commit the mutation
		in the current state. */
	virtual void updateValidate(Mutation *mutation,
			std::function<void(Error)> callback) = 0;
	/* validates a mutation against another mutation.
		returning success mean that it is okay to commit the mutation
		after the other mutation was commited. */
	virtual void updateConflicts(Mutation *mutation,
			Mutation &predecessor,
			std::function<void(Error)> callback) = 0;

	/* commits an mutation to the storage */
	virtual void processUpdate(Mutation *mutation,
			std::function<void(Error)> callback) = 0;
	/* processes a query */
	virtual void processFetch(FetchRequest *fetch,
			std::function<void(FetchData &)> on_data,
			std::function<void(Error)> callback) = 0;

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

