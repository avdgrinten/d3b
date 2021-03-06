
#include <atomic>

#include "ll/random-access-file.hpp"
#include "ll/btree.hpp"

namespace Db {

class FlexStorage : public QueuedStorageDriver {
public:
	class Factory : public StorageDriver::Factory {
	public:
		Factory();
		
		virtual StorageDriver *newDriver(Engine *engine);
	};
	
	FlexStorage(Engine *engine);

	virtual void createStorage();
	virtual void loadStorage();

	virtual DocumentId allocate();
	
	virtual void reinspect(Mutation &mutation);
	
protected:
	virtual void processInsert(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback);
	virtual void processModify(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback);

	virtual void processFetch(FetchRequest *fetch,
			Async::Callback<void(FetchData &)> on_data,
			Async::Callback<void(FetchError)> callback);

private:
	struct Index {
		enum Fields {
			kDocumentId = 0,
			kSequenceId = 8,
			// size of the whole structure
			kStructSize = 16
		};

		DocumentId documentId;
		SequenceId sequenceId;
	};
	struct Reference {
		enum Fields {
			kOffset = 0,
			kLength = 8,
			// size of the whole structure
			kStructSize = 16
		};
		
		size_t offset;
		size_t length;
	};

	void writeIndex(void *buffer, const Index &index);
	Index readIndex(const void *buffer);
	
	std::atomic<DocumentId> p_lastDocumentId;
	size_t p_dataPointer;
	
	Btree<Index> p_indexTree;
	Ll::RandomAccessFile p_dataFile;

	class InsertClosure {
	public:
		InsertClosure(FlexStorage *storage, DocumentId document_id,
				SequenceId sequence_id, std::string buffer,
				Async::Callback<void(Error)> callback);

		void apply();
	
	private:
		void compareToInserted(const Index &other,
				Async::Callback<void(int)> callback);
		void onDataWrite();
		void onIndexInsert();

		FlexStorage *p_storage;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		std::string p_buffer;
		Async::Callback<void(Error)> p_callback;
		
		Index p_index;
		char p_refBuffer[Reference::kStructSize];
		
		Ll::RandomAccessFile::WriteClosure p_dataWrite;
	};

	class FetchClosure {
	public:
		FetchClosure(FlexStorage *storage, DocumentId document_id,
				SequenceId sequence_id,
				Async::Callback<void(FetchData &)> on_data,
				Async::Callback<void(FetchError)> callback);

		void process();
	
	private:
		void compareToFetched(const Index &other,
				Async::Callback<void(int)> callback);
		void onIndexFound(Btree<Index>::Ref ref);
		void onSeek();
		void onDataRead();

		FlexStorage *p_storage;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		Async::Callback<void(FetchData &)> p_onData;
		Async::Callback<void(FetchError)> p_callback;

		FetchData p_fetchData;

		Ll::RandomAccessFile::ReadClosure p_dataRead;
		Btree<Index>::FindClosure p_btreeFind;
		Btree<Index>::IterateClosure p_btreeIterate;
	};
};

}

