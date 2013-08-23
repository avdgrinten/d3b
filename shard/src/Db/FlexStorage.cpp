
#include <cstdint>
#include <string>

#include "Os/Linux.h"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "Db/FlexStorage.hpp"

namespace Db {

FlexStorage::FlexStorage(Engine *engine)
		: StorageDriver(engine), p_docStore("documents") {
}

void FlexStorage::createStorage() {
	p_docStore.setPath(this->getPath());
	p_docStore.createStore();
}

void FlexStorage::loadStorage() {
	p_docStore.setPath(this->getPath());
	p_docStore.loadStore();
}

Proto::StorageConfig FlexStorage::writeConfig() {
	Proto::StorageConfig config;
	config.set_driver("FlexStorage");
	config.set_identifier(getIdentifier());
	return config;
}
void FlexStorage::readConfig(const Proto::StorageConfig &config) {
	
}

void FlexStorage::updateAccept(Proto::Update *update,
		std::function<void(Error)> callback) {
	if(update->action() == Proto::kActInsert) {
		DataStore::Object object = p_docStore.createObject();
		update->set_id(p_docStore.objectLid(object));
		
		callback(Error(true));
	}else if(update->action() == Proto::kActUpdate) {
		callback(Error(true));
	}else{
		throw std::logic_error("Illegal update");
	}
}

void FlexStorage::updateValidate(Proto::Update *update,
		std::function<void(Error)> callback) {
	/* TODO */
	callback(Error(true));
}
void FlexStorage::updateConflicts(Proto::Update *update,
		Proto::Update &predecessor,
		std::function<void(Error)> callback) {
	/* TODO */
	callback(Error(true));
}

void FlexStorage::processUpdate(Proto::Update *update,
		std::function<void(Error)> callback) {
	if(update->action() == Proto::kActInsert) {
		if(!update->has_id() || !update->has_buffer())
			throw std::logic_error("Incomplete kActInsert update");
		DataStore::Object object = p_docStore.getObject(update->id());
		p_docStore.allocObject(object, update->buffer().size());
		p_docStore.writeObject(object, 0, update->buffer().size(),
				update->buffer().data());
		callback(Error(true));
	}else if(update->action() == Proto::kActUpdate) {
		if(!update->has_id() || !update->has_buffer())
			throw std::logic_error("Incomplete kActUpdate update");
		DataStore::Object object = p_docStore.getObject(update->id());
		p_docStore.allocObject(object, update->buffer().size());
		p_docStore.writeObject(object, 0, update->buffer().size(),
				update->buffer().data());
		callback(Error(true));
	}else{
		throw std::logic_error("Illegal update");
	}
}
void FlexStorage::processFetch(Proto::Fetch *fetch,
		std::function<void(Proto::FetchData &)> on_data,
		std::function<void(Error)> callback) {
	DataStore::Object object = p_docStore.getObject(fetch->id());
	
	Linux::size_type length = p_docStore.objectLength(object);
	char *buffer = new char[length];
	p_docStore.readObject(object, 0, length, buffer);
	
	Proto::FetchData result;
	result.set_id(fetch->id());
	result.set_buffer(std::string(buffer, length));
	on_data(result);
	
	delete[] buffer;
	callback(Error(true));
}

FlexStorage::Factory::Factory()
		: StorageDriver::Factory("FlexStorage") {
}

StorageDriver *FlexStorage::Factory::newDriver(Engine *engine) {
	return new FlexStorage(engine);
}

};

