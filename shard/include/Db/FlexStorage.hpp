
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

	virtual void updateAccept(Proto::Update *update,
			std::function<void(Error)> callback);
	virtual void updateValidate(Proto::Update *update,
			std::function<void(Error)> callback);
	virtual void updateConflicts(Proto::Update *update,
			Proto::Update &predecessor,
			std::function<void(Error)> callback);

	virtual void processUpdate(Proto::Update *update,
			std::function<void(Error)> callback);
	virtual void processFetch(FetchRequest *fetch,
			std::function<void(FetchData &)> on_data,
			std::function<void(Error)> callback);

private:
	DataStore p_docStore;
};

}
