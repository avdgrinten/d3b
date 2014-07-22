
#include <cassert>
#include <cstring>
#include <v8.h>

#include "Ll/DataStore.hpp"
#include "Ll/Btree.hpp"

namespace Db {

class JsView : public ViewDriver {
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
	
	virtual void sequence(std::vector<Mutation *> &mutations);

	virtual void processQuery(Query *request,
			Async::Callback<void(QueryData &)> report,
			Async::Callback<void(Error)> callback);

private:
	std::string p_scriptFile;
	std::string p_storageName;

	int p_storage;
	DataStore p_keyStore;
	Btree<DocumentId> p_orderTree;
	v8::Persistent<v8::ObjectTemplate> p_global;
	v8::Persistent<v8::Context> p_context;

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
		QueryClosure(JsView *view, Query *query,
				Async::Callback<void(QueryData &)> on_data,
				Async::Callback<void(Error)> on_complete);
		
		void process();
	
	private:
		int compareToBegin(const DocumentId &id);
		void onFindBegin(Btree<DocumentId>::Ref ref);
		void fetchItem();
		void fetchItemLoop();
		void onFetchData(FetchData &data);
		void onFetchComplete(Error error);
		void complete();

		JsView *p_view;
		Query *p_query;
		Async::Callback<void(QueryData &)> p_onData;
		Async::Callback<void(Error)> p_onComplete;

		v8::Persistent<v8::Value> p_beginKey;
		v8::Persistent<v8::Value> p_endKey;
		FetchRequest p_fetch;
		QueryData p_queryData;
		int p_fetchedCount;

		Btree<DocumentId>::FindClosure p_btreeFind;
		Btree<DocumentId>::IterateClosure p_btreeIterate;
	};

	class SequenceClosure {
	public:
		SequenceClosure(JsView *view, std::vector<Mutation *> &mutations);

		void apply();
	
	private:
		void insertOnComplete(Error error);
		void modifyOnRemove(Error error);
		void modifyOnInsert(Error error);
		void complete();

		JsView *p_view;
		std::vector<Mutation *> p_mutations;

		int p_index;
	};

	class InsertClosure {
	public:
		InsertClosure(JsView *view, DocumentId document_id, std::string buffer,
			Async::Callback<void(Error)> callback);
		
		void apply();

	private:
		int compareToNew(const DocumentId &id);
		void onComplete();

		JsView *p_view;
		DocumentId p_documentId;
		std::string p_buffer;
		Async::Callback<void(Error)> p_callback;

		v8::Persistent<v8::Value> p_newKey;
		DocumentId p_insertKey;
		Btree<DocumentId>::InsertClosure p_btreeInsert;
	};
	
	class RemoveClosure {
	public:
		RemoveClosure(JsView *view, DocumentId document_id,
			Async::Callback<void(Error)> callback);
		
		void apply();

	private:
		int compareToRemoved(const DocumentId &id);
		void onFindRemoved(Btree<DocumentId>::Ref ref);
		void processItem();
		void onFetchData(FetchData &data);
		void onFetchComplete(Error error);

		JsView *p_view;
		DocumentId p_documentId;
		Async::Callback<void(Error)> p_callback;
		
		FetchRequest p_fetch;
		v8::Persistent<v8::Value> p_removedKey;

		Btree<DocumentId>::FindClosure p_btreeFind;
		Btree<DocumentId>::IterateClosure p_btreeIterate;
	};
};

}

