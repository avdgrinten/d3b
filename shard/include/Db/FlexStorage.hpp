
#include "Ll/DataStore.hpp"

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

	virtual DocumentId allocate();
	virtual void sequence(std::vector<Mutation> &mutations,
			Async::Callback<void()> callback);
	virtual void processFetch(FetchRequest *fetch,
			Async::Callback<void(FetchData &)> on_data,
			Async::Callback<void(Error)> callback);

private:
	DataStore p_docStore;
};

}

