
#include "Ll/DataStore.h"

namespace Db {

class FlexStorage : public StorageDriver {
public:
	class Factory : public StorageDriver::Factory {
	public:
		Factory();
		
		virtual StorageDriver *newDriver(Engine *engine);
	};
	
	FlexStorage(Engine *engine);

	virtual void createStorage();
	virtual void loadStorage();
	
	virtual Proto::StorageConfig writeConfig();
	virtual void readConfig(const Proto::StorageConfig &config);

	virtual void updateAccept(Mutation *mutation,
			std::function<void(Error)> callback);
	virtual void updateValidate(Mutation *mutation,
			std::function<void(Error)> callback);
	virtual void updateConflicts(Mutation *mutation,
			Mutation &predecessor,
			std::function<void(Error)> callback);

	virtual void processUpdate(Mutation *mutation,
			std::function<void(Error)> callback);
	virtual void processFetch(FetchRequest *fetch,
			std::function<void(FetchData &)> on_data,
			std::function<void(Error)> callback);

private:
	DataStore p_docStore;
};

}

