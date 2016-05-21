

#include <cstdint>
#include <array>
#include <string>
#include <iostream>

#include "Async.hpp"
#include "Os/Linux.hpp"
#include "Ll/Tasks.hpp"

#include "Db/Types.hpp"
#include "Db/StorageDriver.hpp"
#include "Db/ViewDriver.hpp"
#include "Db/Engine.hpp"

#include "Db/JsView.hpp"

std::string exceptString(const v8::Handle<v8::Message> message) {
	v8::String::Utf8Value text(message->Get());
	return std::string(*text, text.length()) + " in line "
			+ std::to_string(message->GetLineNumber());
}

namespace Db {

JsView::JsView(Engine *engine)
	: QueuedViewDriver(engine),
		p_keyFile("keys", engine->getCacheHost(), engine->getIoPool()),
		p_orderTree("order", 4096, KeyRef::kStructSize, Link::kStructSize,
			engine->getCacheHost(), engine->getIoPool()),
		p_keyPointer(0) {
	p_orderTree.setWriteKey(ASYNC_MEMBER(this, &JsView::writeKeyRef));
	p_orderTree.setReadKey(ASYNC_MEMBER(this, &JsView::readKeyRef));
}

void JsView::writeKeyRef(void *buffer, const KeyRef &keyref) {
	OS::packLe64((char *)buffer + KeyRef::kOffset, keyref.offset);
	OS::packLe32((char *)buffer + KeyRef::kLength, keyref.length);
}
JsView::KeyRef JsView::readKeyRef(const void *buffer) {
	KeyRef keyref;
	keyref.offset = OS::unpackLe64((char *)buffer + KeyRef::kOffset);
	keyref.length = OS::unpackLe32((char *)buffer + KeyRef::kLength);
	return keyref;
}

void JsView::createView(const Proto::ViewConfig &config) {
	if(!config.has_base_storage()
			|| !config.has_script_file())
		throw std::runtime_error("Illegal configuration for JsView");
	p_storageName = config.base_storage();
	p_scriptFile = config.script_file();

	OS::writeFileSync(getPath() + "/config", config.SerializeAsString());

	p_storage = getEngine()->getStorage(p_storageName);
	
	for(int i = 0; i < 5; i++)
		p_idleInstances.push(new JsInstance(p_path + "/../../extern/" + p_scriptFile));
	
	p_keyFile.setPath(getPath());
	p_keyFile.createFile();

	p_orderTree.setPath(getPath());
	p_orderTree.createTree();

	processQueue();
}

void JsView::loadView() {
	Proto::ViewConfig config;
	config.ParseFromString(OS::readFileSync(getPath() + "/config"));

	if(!config.has_base_storage()
			|| !config.has_script_file())
		throw std::runtime_error("Illegal configuration for JsView");
	p_storageName = config.base_storage();
	p_scriptFile = config.script_file();

	p_storage = getEngine()->getStorage(p_storageName);
	
	for(int i = 0; i < 5; i++)
		p_idleInstances.push(new JsInstance(p_path + "/../../extern/" + p_scriptFile));

	p_keyFile.setPath(getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_keyFile.createFile();

	p_orderTree.setPath(getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_orderTree.createTree();

	processQueue();
}

void JsView::reinspect(Mutation &mutation) {
	// we don't have to do anything here
}

void JsView::processInsert(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
	if(mutation.storageIndex != p_storage) {
		callback(Error(true));
		return;
	}

	auto closure = new InsertClosure(this, mutation.documentId,
			sequence_id, mutation.buffer, callback);
	closure->apply();
}

void JsView::processModify(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
	if(mutation.storageIndex != p_storage) {
		callback(Error(true));
		return;
	}

	auto closure = new InsertClosure(this, mutation.documentId,
			sequence_id, mutation.buffer, callback);
	closure->apply();
}

void JsView::processQuery(QueryRequest *request,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(QueryError)> on_complete) {
	auto closure = new QueryClosure(this, request, on_data, on_complete);
	closure->process();
}

void JsView::grabInstance(Async::Callback<void(JsInstance *)> callback) {
	std::unique_lock<std::mutex> lock(p_mutex);

	if(p_idleInstances.empty()) {
		p_waitForInstance.push(callback);
	}else{
		JsInstance *instance = p_idleInstances.top();
		p_idleInstances.pop();

		lock.unlock();
		callback(instance);
	}
}

void JsView::releaseInstance(JsInstance *instance) {
	std::unique_lock<std::mutex> lock(p_mutex);

	if(p_waitForInstance.empty()) {
		p_idleInstances.push(instance);
	}else{
		Async::Callback<void(JsInstance *)> callback = p_waitForInstance.front();
		p_waitForInstance.pop();

		lock.unlock();
		callback(instance);
	}
}

// --------------------------------------------------------
// JsView::Factory
// --------------------------------------------------------

JsView::Factory::Factory()
		: ViewDriver::Factory("JsView") {
}

ViewDriver *JsView::Factory::newDriver(Engine *engine) {
	return new JsView(engine);
}

// --------------------------------------------------------
// JsView::JsInstance
// --------------------------------------------------------

JsView::JsInstance::JsInstance(const std::string &script_path)
		: p_enableLog(false) {

	struct ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
		void *Allocate(size_t length) override {
			auto pointer = AllocateUninitialized(length);
			if(pointer)
				memset(pointer, 0, length);
			return pointer;
		}

		void *AllocateUninitialized(size_t length) override {
			return malloc(length);
		}

		void Free(void *data, size_t length) override {
			free(data);
		}
	};

	v8::Isolate::CreateParams params;
	params.array_buffer_allocator = new ArrayBufferAllocator();
	p_isolate = v8::Isolate::New(params);

	v8::Locker locker(p_isolate);
	v8::Isolate::Scope isolate_scope(p_isolate);
	v8::HandleScope handle_scope(p_isolate);
	
	// setup the global object and create a context
	v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	v8::Local<v8::External> js_this = v8::External::New(p_isolate, this);
	global->Set(v8::String::NewFromUtf8(p_isolate, "log", v8::NewStringType::kNormal).ToLocalChecked(),
		v8::FunctionTemplate::New(p_isolate, &JsInstance::jsLog, js_this));
	global->Set(v8::String::NewFromUtf8(p_isolate, "hook", v8::NewStringType::kNormal).ToLocalChecked(),
		v8::FunctionTemplate::New(p_isolate, &JsInstance::jsHook, js_this));
	
	p_context = v8::Global<v8::Context>(p_isolate, v8::Context::New(p_isolate, nullptr, global));

	v8::Context::Scope context_scope(p_context.Get(p_isolate));
	
	// load the view script from a file
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(script_path, Linux::FileMode::read);
	
	Linux::size_type length = file->lengthSync();
	char *buffer = new char[length];
	file->preadSync(0, length, buffer);
	file->closeSync();
	
	v8::Local<v8::String> source = v8::String::NewFromUtf8(p_isolate,
		buffer, v8::NewStringType::kNormal, length).ToLocalChecked();
	delete[] buffer;
	
	// compile the view script and run it
	v8::TryCatch except;
	v8::Local<v8::Script> script = v8::Script::Compile(source);
	if(except.HasCaught())
		throw std::runtime_error(std::string("Exception while compiling script:\n")
				+ exceptString(except.Message()));
	script->Run();
	if(except.HasCaught())
		throw std::runtime_error(std::string("Exception while executing script:\n")
				+ exceptString(except.Message()));
}

v8::Local<v8::Value> JsView::JsInstance::extractDoc(DocumentId id,
		const void *buffer, Linux::size_type length) {
	std::array<v8::Handle<v8::Value>, 2> args;
	args[0] = v8::Number::New(p_isolate, id);
	args[1] = v8::String::NewFromUtf8(p_isolate,
			(char*)buffer, v8::NewStringType::kNormal, length).ToLocalChecked();
	auto result = p_hookExtractDoc.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::extractKey(const void *buffer,
		Linux::size_type length) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = v8::String::NewFromUtf8(p_isolate,
			(char*)buffer, v8::NewStringType::kNormal, length).ToLocalChecked();
	auto result = p_hookExtractKey.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::keyOf(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	auto result = p_hookKeyOf.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::compare(v8::Handle<v8::Value> a,
		v8::Handle<v8::Value> b) {
	std::array<v8::Handle<v8::Value>, 2> args;
	args[0] = a;
	args[1] = b;
	auto result = p_hookCompare.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::serializeKey(v8::Handle<v8::Value> key) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = key;
	auto result = p_hookSerializeKey.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::deserializeKey(v8::Handle<v8::Value> buffer) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = buffer;
	auto result = p_hookDeserializeKey.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::report(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	auto result = p_hookReport.Get(p_isolate)->Call(p_context.Get(p_isolate)->Global(),
			args.size(), args.data());
	return result;
}

void JsView::JsInstance::jsLog(const v8::FunctionCallbackInfo<v8::Value> &info) {
	JsInstance *thisptr = (JsInstance*)v8::Local<v8::External>::Cast(info.Data())->Value();
	if(thisptr->p_enableLog) {
		for(int i = 0; i < info.Length(); i++) {
			v8::String::Utf8Value utf8(info[i]);
			printf("%s", *utf8);
		}
		printf("\n");
	}
}

void JsView::JsInstance::jsHook(const v8::FunctionCallbackInfo<v8::Value> &info) {
	if(info.Length() != 2)
		throw std::runtime_error("hook() expects exactly two arguments");
	JsInstance *thisptr = (JsInstance*)v8::Local<v8::External>::Cast(info.Data())->Value();

	auto func = v8::Local<v8::Function>::Cast(info[1]);

	v8::String::Utf8Value hook_utf8(info[0]);
	std::string hook_name(*hook_utf8, hook_utf8.length());
	if(hook_name == "extractDoc") {
		thisptr->p_hookExtractDoc = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "extractKey") {
		thisptr->p_hookExtractKey = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "keyOf") {
		thisptr->p_hookKeyOf = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "compare") {
		thisptr->p_hookCompare = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "serializeKey") {
		thisptr->p_hookSerializeKey = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "deserializeKey") {
		thisptr->p_hookDeserializeKey = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else if(hook_name == "report") {
		thisptr->p_hookReport = v8::Global<v8::Function>(thisptr->p_isolate, func);
	}else throw std::runtime_error("Illegal hook '" + hook_name + "'");
}

// --------------------------------------------------------
// JsView::JsScope
// --------------------------------------------------------

JsView::JsScope::JsScope(JsInstance &instance)
: p_locker(instance.p_isolate), p_isolateScope(instance.p_isolate),
		p_handleScope(instance.p_isolate),
		p_contextScope(instance.p_context.Get(instance.p_isolate)) { }

// --------------------------------------------------------
// JsView::InsertClosure
// --------------------------------------------------------

JsView::InsertClosure::InsertClosure(JsView *view, DocumentId document_id,
		SequenceId sequence_id, std::string buffer,
		Async::Callback<void(Error)> callback)
	: p_view(view), p_documentId(document_id),
		p_sequenceId(sequence_id), p_buffer(buffer), p_callback(callback),
		p_keyWrite(&view->p_keyFile), p_keyRead(&view->p_keyFile),
		p_btreeInsert(&view->p_orderTree) {
}

void JsView::InsertClosure::apply() {
	p_view->grabInstance(ASYNC_MEMBER(this, &InsertClosure::acquireInstance));
}

void JsView::InsertClosure::acquireInstance(JsInstance *instance) {
	p_instance = instance;

	JsScope scope(*p_instance);
	
	v8::Local<v8::Value> extracted = p_instance->extractDoc(p_documentId,
		p_buffer.data(), p_buffer.length());
	p_newKey = v8::Global<v8::Value>(v8::Isolate::GetCurrent(), p_instance->keyOf(extracted));

	v8::Local<v8::Value> ser = p_instance->serializeKey(p_newKey.Get(v8::Isolate::GetCurrent()));
	v8::String::Utf8Value ser_value(ser);	
	
	p_insertKey.offset = p_view->p_keyPointer;
	p_insertKey.length = ser_value.length();
	p_view->p_keyPointer += ser_value.length();
	p_keyWrite.write(p_insertKey.offset, p_insertKey.length, *ser_value,
			ASYNC_MEMBER(this, &InsertClosure::afterWriteKey));
}
void JsView::InsertClosure::afterWriteKey() {
	OS::packLe64(p_linkBuffer + Link::kDocumentId, p_documentId);
	OS::packLe64(p_linkBuffer + Link::kSequenceId, p_sequenceId);

	p_btreeInsert.insert(&p_insertKey, p_linkBuffer,
		ASYNC_MEMBER(this, &InsertClosure::compareToNew),
		ASYNC_MEMBER(this, &InsertClosure::onComplete));
}
void JsView::InsertClosure::compareToNew(const KeyRef &keyref,
		Async::Callback<void(int)> callback) {
	p_compareBuffer = new char[keyref.length];
	p_compareLength = keyref.length;
	p_compareCallback = callback;
	p_keyRead.read(keyref.offset, keyref.length, p_compareBuffer,
			ASYNC_MEMBER(this, &InsertClosure::doCompareToNew));
}
void JsView::InsertClosure::doCompareToNew() {
	JsScope scope(*p_instance);
	
	v8::Local<v8::Value> ser_a = v8::String::NewFromUtf8(v8::Isolate::GetCurrent(),
			p_compareBuffer, v8::NewStringType::kNormal, p_compareLength).ToLocalChecked();
	delete[] p_compareBuffer;
	v8::Local<v8::Value> key_a = p_instance->deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_instance->compare(key_a, p_newKey.Get(v8::Isolate::GetCurrent()));
	p_compareCallback(result->Int32Value());
}
void JsView::InsertClosure::onComplete() {
	p_view->releaseInstance(p_instance);
	p_callback(Error(true));
	delete this;
}



// --------------------------------------------------------
// JsView::QueryClosure
// --------------------------------------------------------

JsView::QueryClosure::QueryClosure(JsView *view, QueryRequest *query,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(QueryError)> on_complete)
	: p_view(view), p_query(query), p_onData(on_data),
		p_onComplete(on_complete), p_keyRead(&view->p_keyFile),
		p_btreeFind(&view->p_orderTree),
		p_btreeIterate(&view->p_orderTree) {
}

void JsView::QueryClosure::process() {
	p_view->grabInstance(ASYNC_MEMBER(this, &QueryClosure::acquireInstance));
}

void JsView::QueryClosure::acquireInstance(JsInstance *instance) {
	p_instance = instance;

	JsScope scope(*p_instance);

	if(p_query->useToKey)
		p_endKey = v8::Global<v8::Value>(v8::Isolate::GetCurrent(),
				p_instance->extractKey(p_query->toKey.c_str(), p_query->toKey.size()));
	
	Btree<KeyRef>::Ref begin_ref;
	if(p_query->useFromKey) {
		p_beginKey = v8::Global<v8::Value>(v8::Isolate::GetCurrent(),
				p_instance->extractKey(p_query->fromKey.c_str(), p_query->fromKey.size()));
		p_btreeFind.findNext(ASYNC_MEMBER(this, &QueryClosure::compareToBegin),
				ASYNC_MEMBER(this, &QueryClosure::onFindBegin));
	}else{
		p_btreeFind.findFirst(ASYNC_MEMBER(this, &QueryClosure::onFindBegin));
	}
}
void JsView::QueryClosure::onFindBegin(Btree<KeyRef>::Ref ref) {
	p_btreeIterate.seek(ref, ASYNC_MEMBER(this, &QueryClosure::fetchItem));
}
void JsView::QueryClosure::compareToBegin(const KeyRef &keyref,
		Async::Callback<void(int)> callback) {
	p_compareBuffer = new char[keyref.length];
	p_compareLength = keyref.length;
	p_compareCallback = callback;
	p_keyRead.read(keyref.offset, keyref.length, p_compareBuffer,
			ASYNC_MEMBER(this, &QueryClosure::doCompareToBegin));
}
void JsView::QueryClosure::doCompareToBegin() {
	JsScope scope(*p_instance);

	v8::Local<v8::Value> ser_a = v8::String::NewFromUtf8(v8::Isolate::GetCurrent(),
			p_compareBuffer, v8::NewStringType::kNormal, p_compareLength).ToLocalChecked();
	delete[] p_compareBuffer;
	v8::Local<v8::Value> key_a = p_instance->deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_instance->compare(key_a, p_beginKey.Get(v8::Isolate::GetCurrent()));
	p_compareCallback(result->Int32Value());
}
void JsView::QueryClosure::fetchItem() {
	if(!p_btreeIterate.valid()) {
		complete();
		return;
	}
	if(p_query->limit != -1 && p_fetchedCount == p_query->limit) {
		complete();
		return;
	}

	char link_buffer[Link::kStructSize];
	p_btreeIterate.getValue(link_buffer);
	DocumentId id = OS::unpackLe64(link_buffer + Link::kDocumentId);
	p_expectedSequenceId = OS::unpackLe64(link_buffer + Link::kSequenceId);

	// skip removed documents
	if(id == 0) {
		p_btreeIterate.forward(ASYNC_MEMBER(this, &QueryClosure::fetchItemLoop));
		return;
	}
	
	p_fetch.storageIndex = p_view->p_storage;
	p_fetch.documentId = id;
	p_fetch.sequenceId = p_query->sequenceId;
	p_view->getEngine()->fetch(&p_fetch,
			ASYNC_MEMBER(this, &QueryClosure::onFetchData),
			ASYNC_MEMBER(this, &QueryClosure::onFetchComplete));
}
void JsView::QueryClosure::fetchItemLoop() {
	LocalTaskQueue::get()->submit(ASYNC_MEMBER(this, &QueryClosure::fetchItem));
}
void JsView::QueryClosure::onFetchData(FetchData &data) {
	// make sure we only return the last version of each document
	if(data.sequenceId != p_expectedSequenceId)
		return;
	
	JsScope scope(*p_instance);
	
	v8::Local<v8::Value> extracted = p_instance->extractDoc(data.documentId,
			data.buffer.data(), data.buffer.length());

	/* FIXME:
	if(!p_endKey.IsEmpty()) {
		v8::Local<v8::Value> key = p_instance->keyOf(extracted);
		if(p_instance->compare(key, p_endKey)->Int32Value() > 0)
			break;
	}*/
	
	v8::Local<v8::Value> rpt_value = p_instance->report(extracted);
	v8::String::Utf8Value rpt_utf8(rpt_value);
	p_queryData.items.emplace_back(*rpt_utf8, rpt_utf8.length());
	
	++p_fetchedCount;
}
void JsView::QueryClosure::onFetchComplete(FetchError error) {
	if(error == kFetchSuccess) {
		// everything is okay
	}else throw std::logic_error("Unexpected error during fetch");

	if(p_queryData.items.size() >= 1000) {
		p_onData(p_queryData);
		p_queryData.items.clear();
	}

	p_btreeIterate.forward(ASYNC_MEMBER(this, &QueryClosure::fetchItemLoop));
}
void JsView::QueryClosure::complete() {
	if(p_queryData.items.size() > 0)
		p_onData(p_queryData);

	p_view->releaseInstance(p_instance);
	p_view->finishRequest();
	p_onComplete(kQuerySuccess);
	delete this;
}

}; // namespace Db

