
#include "Ll/Btree.hpp"

namespace Db {

class FlexStorage : public StorageDriver {
public:
	class Factory : public StorageDriver::Factory {
	public:
		Factory();
		
		virtual StorageDriver *newDriver(Engine *engine);
	};
	
	FlexStorage(Engine *engine);

	virtual void createStorage();
	virtual void loadStorage();
	
	virtual Proto::StorageConfig writeConfig();
	virtual void readConfig(const Proto::StorageConfig &config);

	virtual DocumentId allocate();
	virtual void sequence(SequenceId sequence_id,
			std::vector<Mutation> &mutations,
			Async::Callback<void()> callback);
	virtual void processFetch(FetchRequest *fetch,
			Async::Callback<void(FetchData &)> on_data,
			Async::Callback<void(Error)> callback);

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
	
	SequenceId p_currentSequenceId;
	DocumentId p_lastDocumentId;
	size_t p_dataPointer;
	
	Btree<Index> p_indexTree;
	std::unique_ptr<Linux::File> p_dataFile;
	
	class SequenceClosure {
	public:
		SequenceClosure(FlexStorage *storage,
				SequenceId sequence_id,
				std::vector<Mutation> &mutations,
				Async::Callback<void()> callback);

		void apply();
	
	private:
		void onCompleteItem(Error error);
		void complete();

		FlexStorage *p_storage;
		SequenceId p_sequenceId;
		std::vector<Mutation> &p_mutations;
		Async::Callback<void()> p_callback;

		int p_index;
	};

	class InsertClosure {
	public:
		InsertClosure(FlexStorage *storage, DocumentId document_id,
				SequenceId sequence_id, std::string buffer,
				Async::Callback<void(Error)> callback);

		void apply();
	
	private:
		void compareToInserted(const Index &other,
				Async::Callback<void(int)> callback);
		void onIndexInsert();

		FlexStorage *p_storage;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		std::string p_buffer;
		Async::Callback<void(Error)> p_callback;
		
		Index p_index;
		char p_refBuffer[Reference::kStructSize];

		Btree<Index>::InsertClosure p_btreeInsert;
	};

	class FetchClosure {
	public:
		FetchClosure(FlexStorage *storage, DocumentId document_id,
				SequenceId sequence_id,
				Async::Callback<void(FetchData &)> on_data,
				Async::Callback<void(Error)> callback);

		void process();
	
	private:
		void compareToFetched(const Index &other,
				Async::Callback<void(int)> callback);
		void onIndexFound(Btree<Index>::Ref ref);
		void onSeek();

		FlexStorage *p_storage;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		Async::Callback<void(FetchData &)> p_onData;
		Async::Callback<void(Error)> p_callback;

		FetchData p_fetchData;
		size_t p_fetchLength;
		char *p_fetchBuffer;

		Btree<Index>::FindClosure p_btreeFind;
		Btree<Index>::IterateClosure p_btreeIterate;
	};
};

}

