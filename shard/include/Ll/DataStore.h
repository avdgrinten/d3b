
#ifndef D3B_LL_DATASTORE_H_
#define D3B_LL_DATASTORE_H_

class DataStore {
public:
	typedef uint64_t lid_type;

	struct IdxHeader {
		uint32_t numRecords;
		uint32_t dataSize;
	};
	struct IdxRecord {
		uint32_t offset;
		uint32_t length;
	};

	class Object {
	friend class DataStore;
	private:
		lid_type lid;
		IdxRecord index;
	};

	DataStore(const std::string &name);
	
	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}

	void createStore();
	void loadStore();
	
	lid_type numObjects();
	
	Object createObject();
	Object getObject(lid_type lid);

	lid_type objectLid(Object &object);
	Linux::size_type objectLength(Object &object);

	void allocObject(Object &object, Linux::size_type length);
	void writeObject(Object &object, Linux::size_type offset,
			Linux::size_type length, const void *buffer);
	void readObject(Object &object, Linux::size_type offset,
			Linux::size_type length, void *buffer);
	
private:
	IdxHeader p_curIdxHeader;

	std::string p_path;
	std::string p_storeName;

	std::unique_ptr<Linux::File> p_fIndex;
	std::unique_ptr<Linux::File> p_fData;
	
	void p_writeIdxHeader();
	void p_readIdxHeader();

	lid_type p_allocIndex();
	Linux::off_type p_allocData(Linux::size_type length);

	void p_writeIndex(lid_type lid, const IdxRecord &index);
	void p_readIndex(lid_type lid, IdxRecord &index);

	void p_writeData(const IdxRecord &index, Linux::size_type length, const void *data);
	void p_readData(const IdxRecord &index, Linux::size_type length, void *data);
};

#endif

