
#include <cstdint>
#include <string>

#include "Os/Linux.h"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Transact.hpp"
#include "Db/Engine.hpp"

#include "Db/FlexImutStorage.hpp"

namespace Db {

FlexImutStorage::FlexImutStorage()
		: p_docStore("documents") {
}

void FlexImutStorage::createStorage() {
	p_docStore.setPath(this->getPath());
	p_docStore.createStore();
}

void FlexImutStorage::loadStorage() {
	p_docStore.setPath(this->getPath());
	p_docStore.loadStore();
}

Proto::StorageConfig FlexImutStorage::writeConfig() {
	Proto::StorageConfig config;
	config.set_driver("FlexImutStorage");
	config.set_identifier(getIdentifier());
	return config;
}
void FlexImutStorage::readConfig(const Proto::StorageConfig &config) {
	
}

Error FlexImutStorage::allocId(id_type *out_id) {
	DataStore::Object object = p_docStore.createObject();
	*out_id = p_docStore.objectLid(object);
	return Error(true);
}

Error FlexImutStorage::insert(id_type id,
		const void *document, Linux::size_type length) {
	DataStore::Object object = p_docStore.getObject(id);
	p_docStore.allocObject(object, length);
	p_docStore.writeObject(object, 0, length, document);
	return Error(true);
}

Linux::size_type FlexImutStorage::length(id_type id) {
	DataStore::Object object = p_docStore.getObject(id);
	return p_docStore.objectLength(object);
}

void FlexImutStorage::fetch(id_type id, void *buffer) {
	DataStore::Object object = p_docStore.getObject(id);
	p_docStore.readObject(object, 0,
			p_docStore.objectLength(object), buffer);
}

FlexImutStorage::Factory::Factory()
		: StorageDriver::Factory("FlexImutStorage") {
}

StorageDriver *FlexImutStorage::Factory::newDriver(Engine *engine) {
	return new FlexImutStorage;
}

};

