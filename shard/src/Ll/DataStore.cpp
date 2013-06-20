
#include "Os/Linux.h"
#include "Ll/DataStore.h"

DataStore::DataStore(const std::string &name) : p_storeName(name) {
	p_fIndex = osIntf->createFile();
	p_fData = osIntf->createFile();
}

void DataStore::createStore() {
	std::string p_index = p_path + "/" + p_storeName + ".index";
	std::string p_data = p_path + "/" + p_storeName + ".data";

	p_fIndex->openSync(p_index, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
	p_fData->openSync(p_data, Linux::kFileCreate | Linux::kFileTrunc
			| Linux::FileMode::read | Linux::FileMode::write);
	
	p_curIdxHeader.numRecords = 0;
	p_curIdxHeader.dataSize = 0;
	p_writeIdxHeader();
}
void DataStore::loadStore() {
	std::string p_index = p_path + "/" + p_storeName + ".index";
	std::string p_data = p_path + "/" + p_storeName + ".data";

	p_fIndex->openSync(p_index, Linux::FileMode::read | Linux::FileMode::write);
	p_fData->openSync(p_data, Linux::FileMode::read | Linux::FileMode::write);
	
	p_readIdxHeader();
}

DataStore::lid_type DataStore::numObjects() {
	return p_curIdxHeader.numRecords;
}

DataStore::Object DataStore::createObject() {
	DataStore::Object object;
	object.lid = p_allocIndex();
	object.index.offset = 0;
	object.index.length = 0;
	return object;
}
DataStore::Object DataStore::getObject(DataStore::lid_type id) {
	DataStore::Object object;
	object.lid = id;
	p_readIndex(object.lid, object.index);
	return object;
}

DataStore::lid_type DataStore::objectLid(DataStore::Object &object) {
	return object.lid;
}
Linux::size_type DataStore::objectLength(DataStore::Object &object) {
	return object.index.length;
}

void DataStore::allocObject(DataStore::Object &object,
		Linux::size_type length) {
	object.index.offset = p_allocData(length);
	object.index.length = length;
	p_writeIndex(object.lid, object.index);
}

void DataStore::writeObject(DataStore::Object &object,
		Linux::off_type offset, Linux::size_type length,
		const void *buffer) {
	p_writeData(object.index, length, ((char*)buffer) + offset);
}
void DataStore::readObject(DataStore::Object &object,
		Linux::off_type offset, Linux::size_type length,
		void *buffer) {
	p_readData(object.index, length, ((char*)buffer) + offset);
}

void DataStore::p_writeIdxHeader() {
	DataStore::IdxHeader disc_header;
	disc_header.numRecords = OS::toLeU32(p_curIdxHeader.numRecords);
	disc_header.dataSize = OS::toLeU32(p_curIdxHeader.dataSize);
	p_fIndex->pwriteSync(0, sizeof(DataStore::IdxHeader), &disc_header);
}
void DataStore::p_readIdxHeader() {
	DataStore::IdxHeader disc_header;
	p_fIndex->preadSync(0, sizeof(DataStore::IdxHeader), &disc_header);
	p_curIdxHeader.numRecords = OS::fromLeU32(disc_header.numRecords);
	p_curIdxHeader.dataSize = OS::fromLeU32(disc_header.dataSize);
}

void DataStore::p_writeIndex(DataStore::lid_type lid,
		const DataStore::IdxRecord &index) {
	DataStore::IdxRecord disc_index;
	disc_index.offset = OS::toLeU32(index.offset);
	disc_index.length = OS::toLeU32(index.length);
	p_fIndex->pwriteSync(sizeof(DataStore::IdxHeader)
			+ (lid - 1) * sizeof(DataStore::IdxRecord),
			sizeof(DataStore::IdxRecord), &disc_index);
}
void DataStore::p_readIndex(DataStore::lid_type lid,
		DataStore::IdxRecord &index) {
	DataStore::IdxRecord disc_index;
	p_fIndex->preadSync(sizeof(DataStore::IdxHeader)
			+ (lid - 1) * sizeof(DataStore::IdxRecord),
			sizeof(DataStore::IdxRecord), &disc_index);
	index.offset = OS::toLeU32(disc_index.offset);
	index.length = OS::toLeU32(disc_index.length);
}

void DataStore::p_writeData(const DataStore::IdxRecord &index,
		Linux::size_type length, const void *data) {
	p_fData->pwriteSync(index.offset, length, data);
}
void DataStore::p_readData(const DataStore::IdxRecord &index,
		Linux::size_type length, void *data) {
	p_fData->preadSync(index.offset, length, data);
}

DataStore::lid_type DataStore::p_allocIndex() {
	/* BEGIN ATOMIC */
	DataStore::lid_type lid = p_curIdxHeader.numRecords + 1;
	p_curIdxHeader.numRecords++;
	p_writeIdxHeader();
	/* END ATOMIC */
	return lid;
}
Linux::off_type DataStore::p_allocData(Linux::size_type length) {
	/* BEGIN ATOMIC */
	Linux::off_type offset = p_curIdxHeader.dataSize;
	p_curIdxHeader.dataSize += length;
	p_writeIdxHeader();
	/* END ATOMIC */
	return offset;
}

