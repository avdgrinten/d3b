
#include <cstdint>
#include <string>

#include "Os/Linux.hpp"
#include "Db/Types.hpp"
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
	//NOTE: to test the durability implementation we always delete the data on load!
	p_docStore.createStore();
}

Proto::StorageConfig FlexStorage::writeConfig() {
	Proto::StorageConfig config;
	config.set_driver("FlexStorage");
	config.set_identifier(getIdentifier());
	return config;
}
void FlexStorage::readConfig(const Proto::StorageConfig &config) {
	
}

DocumentId FlexStorage::allocate() {
	DataStore::Object object = p_docStore.createObject();
	return p_docStore.objectLid(object);
}

void FlexStorage::sequence(std::vector<Mutation *> &mutations) {
	for(auto it = mutations.begin(); it != mutations.end(); ++it) {
		Mutation *mutation = *it;
		if(mutation->type == Mutation::kTypeInsert) {
			DataStore::Object object = p_docStore.getObject(mutation->documentId);
			p_docStore.allocObject(object, mutation->buffer.size());
			p_docStore.writeObject(object, 0, mutation->buffer.size(),
					mutation->buffer.data());
		}else if(mutation->type == Mutation::kTypeModify) {
			DataStore::Object object = p_docStore.getObject(mutation->documentId);
			p_docStore.allocObject(object, mutation->buffer.size());
			p_docStore.writeObject(object, 0, mutation->buffer.size(),
					mutation->buffer.data());
		}else{
			throw std::logic_error("Illegal mutation");
		}
	}
}

void FlexStorage::processFetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(Error)> callback) {
	DataStore::Object object = p_docStore.getObject(fetch->documentId);
	
	Linux::size_type length = p_docStore.objectLength(object);
	char *buffer = new char[length];
	p_docStore.readObject(object, 0, length, buffer);
	
	FetchData result;
	result.documentId = fetch->documentId;
	result.buffer = std::string(buffer, length);
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

