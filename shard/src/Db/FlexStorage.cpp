
#include <cstdint>
#include <string>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "Db/FlexStorage.hpp"

namespace Db {

FlexStorage::FlexStorage(Engine *engine)
		: QueuedStorageDriver(engine), p_lastDocumentId(0), p_dataPointer(0),
			p_indexTree("index", 4096, Index::kStructSize, Reference::kStructSize) {
	p_indexTree.setWriteKey(ASYNC_MEMBER(this, &FlexStorage::writeIndex));
	p_indexTree.setReadKey(ASYNC_MEMBER(this, &FlexStorage::readIndex));

	p_dataFile = osIntf->createFile();
}

void FlexStorage::createStorage() {
	p_indexTree.setPath(this->getPath());
	p_indexTree.createTree();

	std::string data_path = p_path + "/data.bin";
	p_dataFile->openSync(data_path, Linux::kFileCreate | Linux::kFileTrunc
				| Linux::FileMode::read | Linux::FileMode::write);
	
	processQueue();
}

void FlexStorage::loadStorage() {
	p_indexTree.setPath(this->getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_indexTree.createTree();

	std::string data_path = p_path + "/data.bin";
	//NOTE: to test the durability implementation we always delete the data on load!
	p_dataFile->openSync(data_path, Linux::kFileCreate | Linux::kFileTrunc
				| Linux::FileMode::read | Linux::FileMode::write);
	
	processQueue();
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
	p_lastDocumentId++;
	return p_lastDocumentId;
}

void FlexStorage::processInsert(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
	auto closure = new InsertClosure(this, mutation.documentId,
			sequence_id, mutation.buffer, callback);
	closure->apply();
}
void FlexStorage::processModify(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
	auto closure = new InsertClosure(this, mutation.documentId,
			sequence_id, mutation.buffer, callback);
	closure->apply();
}

void FlexStorage::processFetch(FetchRequest *fetch,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(Error)> callback) {
	auto closure = new FetchClosure(this, fetch->documentId,
			fetch->sequenceId, on_data, callback);
	closure->process();
}

void FlexStorage::writeIndex(void *buffer, const Index &index) {
	OS::packLe64((char*)buffer + Index::kDocumentId, index.documentId);
	OS::packLe64((char*)buffer + Index::kSequenceId, index.sequenceId);
}
FlexStorage::Index FlexStorage::readIndex(const void *buffer) {
	Index index;
	index.documentId = OS::unpackLe64((char*)buffer + Index::kDocumentId);
	index.sequenceId = OS::unpackLe64((char*)buffer + Index::kSequenceId);
	return index;
}

FlexStorage::Factory::Factory()
		: StorageDriver::Factory("FlexStorage") {
}

StorageDriver *FlexStorage::Factory::newDriver(Engine *engine) {
	return new FlexStorage(engine);
}

// --------------------------------------------------------
// InsertClosure
// --------------------------------------------------------

FlexStorage::InsertClosure::InsertClosure(FlexStorage *storage,
		DocumentId document_id, SequenceId sequence_id, std::string buffer,
		Async::Callback<void(Error)> callback)
	: p_storage(storage), p_documentId(document_id),
		p_sequenceId(sequence_id), p_buffer(buffer),
		p_callback(callback), p_btreeInsert(&storage->p_indexTree) { }

void FlexStorage::InsertClosure::apply() {
	size_t data_pointer = p_storage->p_dataPointer;
	p_storage->p_dataPointer += p_buffer.size();

	p_storage->p_dataFile->pwriteSync(data_pointer, p_buffer.size(), p_buffer.data());
	
	p_index.documentId = p_documentId;
	p_index.sequenceId = p_sequenceId;
	
	OS::packLe64(p_refBuffer + Reference::kOffset, data_pointer);
	OS::packLe64(p_refBuffer + Reference::kLength, p_buffer.size());
	
	p_btreeInsert.insert(&p_index, p_refBuffer,
			ASYNC_MEMBER(this, &InsertClosure::compareToInserted),
			ASYNC_MEMBER(this, &InsertClosure::onIndexInsert));
}
void FlexStorage::InsertClosure::compareToInserted(const Index &other,
		Async::Callback<void(int)> callback) {
	if(other.documentId < p_index.documentId) {
		callback(-1);
		return;
	}else if(other.documentId > p_index.documentId) {
		callback(1);
		return;
	}
	if(other.sequenceId < p_index.sequenceId) {
		callback(-1);
		return;
	}else if(other.sequenceId > p_index.sequenceId) {
		callback(1);
		return;
	}
	callback(0);
}
void FlexStorage::InsertClosure::onIndexInsert() {
	p_callback(Error(true));
	delete this;
}

// --------------------------------------------------------
// FetchClosure
// --------------------------------------------------------

FlexStorage::FetchClosure::FetchClosure(FlexStorage *storage,
		DocumentId document_id, SequenceId sequence_id,
		Async::Callback<void(FetchData &)> on_data,
		Async::Callback<void(Error)> callback)
	: p_storage(storage), p_documentId(document_id), p_sequenceId(sequence_id),
		p_onData(on_data), p_callback(callback),
		p_btreeFind(&storage->p_indexTree),
		p_btreeIterate(&storage->p_indexTree) { }

void FlexStorage::FetchClosure::process() {
	p_btreeFind.findPrev(ASYNC_MEMBER(this, &FetchClosure::compareToFetched),
			ASYNC_MEMBER(this, &FetchClosure::onIndexFound));
}
void FlexStorage::FetchClosure::compareToFetched(const Index &other,
		Async::Callback<void(int)> callback) {
	if(other.documentId < p_documentId) {
		callback(-1);
		return;
	}else if(other.documentId > p_documentId) {
		callback(1);
		return;
	}
	if(other.sequenceId < p_sequenceId) {
		callback(-1);
		return;
	}else if(other.sequenceId > p_sequenceId) {
		callback(1);
		return;
	}
	callback(0);
}
void FlexStorage::FetchClosure::onIndexFound(Btree<Index>::Ref ref) {
	//TODO: handle missing documents
	if(!ref.valid())
		throw std::runtime_error("Ref not valid!");
	p_btreeIterate.seek(ref, ASYNC_MEMBER(this, &FetchClosure::onSeek));
}
void FlexStorage::FetchClosure::onSeek() {
	Index index = p_btreeIterate.getKey();
	//TODO: handle missing documents
	if(index.documentId != p_documentId)
		throw std::runtime_error("Document not found!");

	char ref_buffer[Reference::kStructSize];
	p_btreeIterate.getValue(&ref_buffer);

	size_t offset = OS::unpackLe64(ref_buffer + Reference::kOffset);
	p_fetchLength = OS::unpackLe64(ref_buffer + Reference::kLength);

	p_fetchBuffer = new char[p_fetchLength];
	p_storage->p_dataFile->preadSync(offset, p_fetchLength, p_fetchBuffer);
	
	p_fetchData.documentId = p_documentId;
	p_fetchData.sequenceId = index.sequenceId;
	p_fetchData.buffer = std::string(p_fetchBuffer, p_fetchLength);
	delete[] p_fetchBuffer;

	p_onData(p_fetchData);

	p_callback(Error(true));
	delete this;
}

}; // namespace Db

