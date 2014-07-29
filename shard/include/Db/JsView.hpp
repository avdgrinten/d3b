
#include <cassert>
#include <cstring>
#include <v8.h>

#include "Ll/DataStore.hpp"
#include "Ll/Btree.hpp"

namespace Db {

class JsView : public QueuedViewDriver {
public:
	class Factory : public ViewDriver::Factory {
	public:
		Factory();
		
		virtual ViewDriver *newDriver(Engine *engine);
	};
	
	JsView(Engine *engine);

	virtual void createView();
	virtual void loadView();

	virtual Proto::ViewConfig writeConfig();
	virtual void readConfig(const Proto::ViewConfig &config);
	
protected:
	virtual void processInsert(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback);
	virtual void processModify(SequenceId sequence_id,
			Mutation &mutation, Async::Callback<void(Error)> callback);
	
	virtual void processQuery(QueryRequest *request,
			Async::Callback<void(QueryData &)> report,
			Async::Callback<void(QueryError)> callback);

private:
	struct Link {
		enum Fields {
			kDocumentId = 0,
			kSequenceId = 8,
			// overall size of this struct
			kStructSize = 16
		};

		DocumentId documentId;
		SequenceId sequenceId;
	};

	std::string p_scriptFile;
	std::string p_storageName;

	int p_storage;
	DataStore p_keyStore;
	Btree<DocumentId> p_orderTree;
	v8::Persistent<v8::ObjectTemplate> p_global;
	v8::Persistent<v8::Context> p_context;

	int compare(const DocumentId &key_a, const DocumentId &key_b);
	void writeKey(void *buffer, const DocumentId &key);
	DocumentId readKey(const void *buffer);

	bool p_enableLog;
	static v8::Handle<v8::Value> p_jsLog(const v8::Arguments &args);
	static v8::Handle<v8::Value> p_jsHook(const v8::Arguments &args);

	v8::Persistent<v8::Function> p_funExtractDoc;
	v8::Persistent<v8::Function> p_funExtractKey;
	v8::Persistent<v8::Function> p_funKeyOf;
	v8::Persistent<v8::Function> p_funCompare;
	v8::Persistent<v8::Function> p_funSerializeKey;
	v8::Persistent<v8::Function> p_funDeserializeKey;
	v8::Persistent<v8::Function> p_funReport;

	void p_setupJs();

	v8::Local<v8::Value> p_extractDoc(DocumentId id,
			const void *buffer, Linux::size_type length);
	v8::Local<v8::Value> p_extractKey(const void *buffer,
			Linux::size_type length);
	v8::Local<v8::Value> p_keyOf(v8::Handle<v8::Value> document);
	v8::Local<v8::Value> p_compare(v8::Handle<v8::Value> a,
			v8::Handle<v8::Value> b);
	v8::Local<v8::Value> p_serializeKey(v8::Handle<v8::Value> key);
	v8::Local<v8::Value> p_deserializeKey(v8::Handle<v8::Value> buffer);
	v8::Local<v8::Value> p_report(v8::Handle<v8::Value> document);

	class QueryClosure {
	public:
		QueryClosure(JsView *view, QueryRequest *query,
				Async::Callback<void(QueryData &)> on_data,
				Async::Callback<void(QueryError)> on_complete);
		
		void process();
	
	private:
		void compareToBegin(const DocumentId &id,
				Async::Callback<void(int)> callback);
		void onFindBegin(Btree<DocumentId>::Ref ref);
		void fetchItem();
		void fetchItemLoop();
		void onFetchData(FetchData &data);
		void onFetchComplete(FetchError error);
		void complete();

		JsView *p_view;
		QueryRequest *p_query;
		Async::Callback<void(QueryData &)> p_onData;
		Async::Callback<void(QueryError)> p_onComplete;

		v8::Persistent<v8::Value> p_beginKey;
		v8::Persistent<v8::Value> p_endKey;
		SequenceId p_expectedSequenceId;
		FetchRequest p_fetch;
		QueryData p_queryData;
		int p_fetchedCount;

		Btree<DocumentId>::FindClosure p_btreeFind;
		Btree<DocumentId>::IterateClosure p_btreeIterate;
	};

	class InsertClosure {
	public:
		InsertClosure(JsView *view, DocumentId document_id,
			SequenceId sequence_id, std::string buffer,
			Async::Callback<void(Error)> callback);
		
		void apply();

	private:
		void compareToNew(const DocumentId &id,
				Async::Callback<void(int)> callback);
		void onComplete();

		JsView *p_view;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		std::string p_buffer;
		Async::Callback<void(Error)> p_callback;

		v8::Persistent<v8::Value> p_newKey;
		char p_linkBuffer[Link::kStructSize];
		DocumentId p_insertKey;

		Btree<DocumentId>::InsertClosure p_btreeInsert;
	};
};

}

