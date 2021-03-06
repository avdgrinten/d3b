
#include <cstdint>
#include <string>
#include <iostream>

#include "async.hpp"
#include "os/linux.hpp"
#include "ll/tasks.hpp"

#include "db/types.hpp"
#include "db/storage-driver.hpp"
#include "db/view-driver.hpp"
#include "db/engine.hpp"

#include "db/flex-storage.hpp"

namespace Db {

FlexStorage::FlexStorage(Engine *engine)
		: QueuedStorageDriver(engine), p_lastDocumentId(0), p_dataPointer(0),
			p_indexTree("index", 4096, Index::kStructSize, Reference::kStructSize,
				engine->getCacheHost(), engine->getIoPool()),
			p_dataFile("data", engine->getCacheHost(), engine->getIoPool()) {
	p_indexTree.setWriteKey(ASYNC_MEMBER(this, &FlexStorage::writeIndex));
	p_indexTree.setReadKey(ASYNC_MEMBER(this, &FlexStorage::readIndex));
}

void FlexStorage::createStorage() {
	p_indexTree.setPath(getPath());
	p_indexTree.createTree();

	p_dataFile.setPath(p_path);
	p_dataFile.createFile();
	
	processQueue();
}

void FlexStorage::loadStorage() {
	p_indexTree.setPath(getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_indexTree.createTree();

	p_dataFile.setPath(p_path);
	//NOTE: to test the durability implementation we always delete the data on load!
	p_dataFile.createFile();
	
	processQueue();
}

DocumentId FlexStorage::allocate() {
	return ++p_lastDocumentId;
}

void FlexStorage::reinspect(Mutation &mutation) {
	if(mutation.type == Mutation::kTypeInsert) {
		if(p_lastDocumentId < mutation.documentId)
			p_lastDocumentId = mutation.documentId;
	}
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
		Async::Callback<void(FetchError)> callback) {
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
		p_callback(callback), p_dataWrite(&storage->p_dataFile) { }

void FlexStorage::InsertClosure::apply() {
	size_t data_pointer = p_storage->p_dataPointer;
	p_storage->p_dataPointer += p_buffer.size();
	
	p_index.documentId = p_documentId;
	p_index.sequenceId = p_sequenceId;
	
	OS::packLe64(p_refBuffer + Reference::kOffset, data_pointer);
	OS::packLe64(p_refBuffer + Reference::kLength, p_buffer.size());
	
	p_dataWrite.write(data_pointer, p_buffer.size(), p_buffer.data(),
			ASYNC_MEMBER(this, &InsertClosure::onDataWrite));
}
void FlexStorage::InsertClosure::onDataWrite() {
	auto action = p_storage->p_indexTree.insert(&p_index, p_refBuffer,
			ASYNC_MEMBER(this, &InsertClosure::compareToInserted));
	libchain::run(action, ASYNC_MEMBER(this, &InsertClosure::onIndexInsert));
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
		Async::Callback<void(FetchError)> callback)
	: p_storage(storage), p_documentId(document_id), p_sequenceId(sequence_id),
		p_onData(on_data), p_callback(callback),
		p_dataRead(&storage->p_dataFile),
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
	if(!ref.valid()) {
		p_callback(kFetchDocumentNotFound);
		p_storage->finishRequest();
		delete this;
		return;
	}

	p_btreeIterate.seek(ref, ASYNC_MEMBER(this, &FetchClosure::onSeek));
}
void FlexStorage::FetchClosure::onSeek() {
	Index index = p_btreeIterate.getKey();
	if(index.documentId != p_documentId) {
		p_callback(kFetchDocumentNotFound);
		p_storage->finishRequest();
		delete this;
		return;
	}

	char ref_buffer[Reference::kStructSize];
	p_btreeIterate.getValue(&ref_buffer);

	size_t offset = OS::unpackLe64(ref_buffer + Reference::kOffset);
	size_t length = OS::unpackLe64(ref_buffer + Reference::kLength);
	
	p_fetchData.documentId = p_documentId;
	p_fetchData.sequenceId = index.sequenceId;
	p_fetchData.buffer.resize(length);

	p_dataRead.read(offset, length, &p_fetchData.buffer[0],
			ASYNC_MEMBER(this, &FetchClosure::onDataRead));
}
void FlexStorage::FetchClosure::onDataRead() {
	p_onData(p_fetchData);

	p_callback(kFetchSuccess);
	p_storage->finishRequest();
	delete this;
}

}; // namespace Db

