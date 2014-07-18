
#include "proto/Config.pb.h"
#include "proto/Request.pb.h"

namespace Db {

/*FIXME: restructure headers */
typedef int64_t id_type;

class Engine;

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

	/* checks whether an update is accepted. fills in
		additional details like the allocated id for insert
		requests */
	virtual void updateAccept(Proto::Update *update,
			std::function<void(Error)> callback) = 0;
	/* validates an update against the current state.
		returning success means that it is okay to commit the update
		in the current state. */
	virtual void updateValidate(Proto::Update *update,
			std::function<void(Error)> callback) = 0;
	/* validates an update against another update.
		returning success mean that it is okay to commit the update
		after the other update was commited. */
	virtual void updateConflicts(Proto::Update *update,
			Proto::Update &predecessor,
			std::function<void(Error)> callback) = 0;

	/* commits an update to the storage */
	virtual void processUpdate(Proto::Update *update,
			std::function<void(Error)> callback) = 0;
	/* processes a query */
	virtual void processFetch(Proto::Fetch *fetch,
			std::function<void(Proto::FetchData &)> on_data,
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

