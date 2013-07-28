
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

	virtual void createStorage() = 0;
	virtual void loadStorage() = 0;

	virtual Proto::StorageConfig writeConfig() = 0;
	virtual void readConfig(const Proto::StorageConfig &config) = 0;

	virtual Error allocId(id_type *out_id) = 0;
	
	virtual Error insert(id_type id,
			const void *document, Linux::size_type length) = 0;
	virtual Error update(id_type id,
			const void *document, Linux::size_type length) = 0;
	
	virtual Linux::size_type length(id_type id) = 0;
	virtual void fetch(id_type id, void *buffer) = 0;
	
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

