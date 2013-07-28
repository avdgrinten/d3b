
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

	virtual Error allocId(id_type *out_id);	
	virtual Error insert(id_type id,
			const void *document, Linux::size_type length);
	virtual Error update(id_type id,
			const void *document, Linux::size_type length);
	virtual Linux::size_type length(id_type id);
	virtual void fetch(id_type id, void *buffer);

private:
	DataStore p_docStore;
};

}

