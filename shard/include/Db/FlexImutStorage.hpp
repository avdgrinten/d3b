
#include "Ll/DataStore.h"

namespace Db {

class FlexImutStorage : public StorageDriver {
public:
	class Factory : public StorageDriver::Factory {
	public:
		Factory();
		
		virtual StorageDriver *newDriver(Engine *engine);
	};
	
	FlexImutStorage();

	virtual void createStorage();
	virtual void loadStorage();
	
	virtual Proto::StorageConfig writeConfig();
	virtual void readConfig(const Proto::StorageConfig &config);

	virtual void updateAccept(Proto::Update &update,
			std::function<void(Error)> callback);
	virtual void updateValidate(Proto::Update &update,
			std::function<void(Error)> callback);
	virtual void updateConflicts(Proto::Update &update,
			Proto::Update &predecessor,
			std::function<void(Error)> callback);

	virtual void processUpdate(Proto::Update &update,
			std::function<void(Error)> callback);
	virtual void processQuery(Proto::Query &query,
			std::function<void(Proto::Data &)> on_data,
			std::function<void(Error)> callback);


	virtual Linux::size_type length(id_type id);
	virtual void fetch(id_type id, void *buffer);

private:
	DataStore p_docStore;
};

}

