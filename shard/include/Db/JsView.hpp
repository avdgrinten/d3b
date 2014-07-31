
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
	class JsInstance;
	class JsScope;

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

	int compare(const DocumentId &key_a, const DocumentId &key_b);
	void writeKey(void *buffer, const DocumentId &key);
	DocumentId readKey(const void *buffer);

	JsInstance *grabInstance();
	void releaseInstance(JsInstance *instance);

	std::string p_scriptFile;
	std::string p_storageName;

	int p_storage;
	DataStore p_keyStore;
	Btree<DocumentId> p_orderTree;
	std::stack<JsInstance *> p_instances;

	class JsInstance {
	friend class JsScope;
	public:
		JsInstance(const std::string &script_path);
	
		v8::Local<v8::Value> extractDoc(DocumentId id,
				const void *buffer, Linux::size_type length);
		v8::Local<v8::Value> extractKey(const void *buffer,
				Linux::size_type length);
		v8::Local<v8::Value> keyOf(v8::Handle<v8::Value> document);
		v8::Local<v8::Value> compare(v8::Handle<v8::Value> a,
				v8::Handle<v8::Value> b);
		v8::Local<v8::Value> serializeKey(v8::Handle<v8::Value> key);
		v8::Local<v8::Value> deserializeKey(v8::Handle<v8::Value> buffer);
		v8::Local<v8::Value> report(v8::Handle<v8::Value> document);

	private:
		static v8::Handle<v8::Value> jsLog(const v8::Arguments &args);
		static v8::Handle<v8::Value> jsHook(const v8::Arguments &args);
		
		v8::Isolate *p_isolate;
		v8::Persistent<v8::Context> p_context;
		
		v8::Persistent<v8::Function> p_hookExtractDoc;
		v8::Persistent<v8::Function> p_hookExtractKey;
		v8::Persistent<v8::Function> p_hookKeyOf;
		v8::Persistent<v8::Function> p_hookCompare;
		v8::Persistent<v8::Function> p_hookSerializeKey;
		v8::Persistent<v8::Function> p_hookDeserializeKey;
		v8::Persistent<v8::Function> p_hookReport;
		bool p_enableLog;
	};

	class JsScope {
	public:
		JsScope(JsInstance &instance);
	
	private:
		v8::Locker p_locker;
		v8::Isolate::Scope p_isolateScope;
		v8::Context::Scope p_contextScope;
		v8::HandleScope p_handleScope;
	};

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

		JsInstance *p_instance;
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

		JsInstance *p_instance;
		v8::Persistent<v8::Value> p_newKey;
		char p_linkBuffer[Link::kStructSize];
		DocumentId p_insertKey;

		Btree<DocumentId>::InsertClosure p_btreeInsert;
	};
};

}

