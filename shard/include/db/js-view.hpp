
#include <cassert>
#include <cstring>
#include <v8.h>

#include "ll/random-access-file.hpp"
#include "ll/btree.hpp"

namespace Db {

class JsView : public QueuedViewDriver {
public:
	class Factory : public ViewDriver::Factory {
	public:
		Factory();
		
		virtual ViewDriver *newDriver(Engine *engine);
	};
	
	JsView(Engine *engine);

	virtual void createView(const Proto::ViewConfig &config);
	virtual void loadView();
	
	virtual void reinspect(Mutation &mutation);
	
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
	
	struct KeyRef {
		enum Fields {
			kOffset = 0,
			kLength = 8,
			// overall size of this struct
			kStructSize = 12
		};

		int64_t offset;
		int32_t length;
	};

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

	void writeKeyRef(void *buffer, const KeyRef &key);
	KeyRef readKeyRef(const void *buffer);

	void grabInstance(Async::Callback<void(JsInstance *)> callback);
	void releaseInstance(JsInstance *instance);

	std::string p_scriptFile;
	std::string p_storageName;

	int p_storage;
	Ll::RandomAccessFile p_keyFile;
	Btree<KeyRef> p_orderTree;
	std::stack<JsInstance *> p_idleInstances;
	std::queue<Async::Callback<void(JsInstance *)>> p_waitForInstance;
	std::mutex p_mutex;
	size_t p_keyPointer;

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
		static void jsLog(const v8::FunctionCallbackInfo<v8::Value> &info);
		static void jsHook(const v8::FunctionCallbackInfo<v8::Value> &info);
		
		v8::Isolate *p_isolate;
		v8::Global<v8::Context> p_context;
		
		v8::Global<v8::Function> p_hookExtractDoc;
		v8::Global<v8::Function> p_hookExtractKey;
		v8::Global<v8::Function> p_hookKeyOf;
		v8::Global<v8::Function> p_hookCompare;
		v8::Global<v8::Function> p_hookSerializeKey;
		v8::Global<v8::Function> p_hookDeserializeKey;
		v8::Global<v8::Function> p_hookReport;
		bool p_enableLog;
	};

	class JsScope {
	public:
		JsScope(JsInstance &instance);
	
	private:
		v8::Locker p_locker;
		v8::Isolate::Scope p_isolateScope;
		v8::HandleScope p_handleScope;
		v8::Context::Scope p_contextScope;
	};

	class QueryClosure {
	public:
		QueryClosure(JsView *view, QueryRequest *query,
				Async::Callback<void(QueryData &)> on_data,
				Async::Callback<void(QueryError)> on_complete);
		
		void process();
	
	private:
		void acquireInstance(JsInstance *instance);
		void compareToBegin(const KeyRef &id,
				Async::Callback<void(int)> callback);
		void doCompareToBegin();
		void onFindBegin(Btree<KeyRef>::Ref ref);
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
		char *p_compareBuffer;
		int32_t p_compareLength;
		Async::Callback<void(int)> p_compareCallback;
		v8::Global<v8::Value> p_beginKey;
		v8::Global<v8::Value> p_endKey;
		SequenceId p_expectedSequenceId;
		FetchRequest p_fetch;
		QueryData p_queryData;
		int p_fetchedCount;

		Ll::RandomAccessFile::ReadClosure p_keyRead;
		Btree<KeyRef>::FindClosure p_btreeFind;
		Btree<KeyRef>::IterateClosure p_btreeIterate;
	};

	class InsertClosure {
	public:
		InsertClosure(JsView *view, DocumentId document_id,
			SequenceId sequence_id, std::string buffer,
			Async::Callback<void(Error)> callback);
		
		void apply();

	private:
		void acquireInstance(JsInstance *instance);
		void afterWriteKey();
		void compareToNew(const KeyRef &id,
				Async::Callback<void(int)> callback);
		void doCompareToNew();
		void onComplete();

		JsView *p_view;
		DocumentId p_documentId;
		SequenceId p_sequenceId;
		std::string p_buffer;
		Async::Callback<void(Error)> p_callback;

		JsInstance *p_instance;
		char *p_compareBuffer;
		int32_t p_compareLength;
		Async::Callback<void(int)> p_compareCallback;
		v8::Global<v8::Value> p_newKey;
		char p_linkBuffer[Link::kStructSize];
		KeyRef p_insertKey;
		
		Ll::RandomAccessFile::WriteClosure p_keyWrite;
		Ll::RandomAccessFile::ReadClosure p_keyRead;
		Btree<KeyRef>::InsertClosure p_btreeInsert;
	};
};

}

