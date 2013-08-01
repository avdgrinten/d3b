
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

void FlexImutStorage::updateAccept(Proto::Update &update,
		std::function<void(Error)> callback) {
	if(update.action() == Proto::kActInsert) {
		DataStore::Object object = p_docStore.createObject();
		update.set_id(p_docStore.objectLid(object));
		
		callback(Error(true));
	}else{
		throw std::logic_error("Illegal update");
	}
}

void FlexImutStorage::updateValidate(Proto::Update &update,
		std::function<void(Error)> callback) {
	/* TODO */
	callback(Error(true));
}
void FlexImutStorage::updateConflicts(Proto::Update &update,
		Proto::Update &predecessor,
		std::function<void(Error)> callback) {
	/* TODO */
	callback(Error(true));
}

void FlexImutStorage::processUpdate(Proto::Update &update,
		std::function<void(Error)> callback) {
	if(update.action() == Proto::kActInsert) {
		if(!update.has_id() || !update.has_buffer())
			throw std::logic_error("Incomplete kActInsert update");
		DataStore::Object object = p_docStore.getObject(update.id());
		p_docStore.allocObject(object, update.buffer().size());
		p_docStore.writeObject(object, 0, update.buffer().size(),
				update.buffer().data());
		callback(Error(true));
	}else{
		throw std::logic_error("Illegal update");
	}
}
void FlexImutStorage::processQuery(Proto::Query &query,
		std::function<void(Proto::Data &)> on_data,
		std::function<void(Error)> callback) {
	throw std::runtime_error("processQuery() called");
}

/*Error FlexImutStorage::update(id_type id,
		const void *document, Linux::size_type length) {
	DataStore::Object object = p_docStore.getObject(id);
	p_docStore.allocObject(object, length);
	p_docStore.writeObject(object, 0, length, document);
	return Error(true);
}*/

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

