

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
	: QueuedViewDriver(engine), p_keyStore("keys"),
		p_orderTree("order", 4096, sizeof(DocumentId), Link::kStructSize,
			engine->getIoPool()) {
	p_orderTree.setWriteKey(ASYNC_MEMBER(this, &JsView::writeKey));
	p_orderTree.setReadKey(ASYNC_MEMBER(this, &JsView::readKey));
}

void JsView::writeKey(void *buffer, const DocumentId &key) {
	*(DocumentId*)buffer = OS::toLe(key);
}
DocumentId JsView::readKey(const void *buffer) {
	return OS::fromLe(*(DocumentId*)buffer);
}

void JsView::createView() {
	p_storage = getEngine()->getStorage(p_storageName);
	
	p_instances.push(new JsInstance(p_path + "/../../extern/" + p_scriptFile));
	
	p_keyStore.setPath(this->getPath());
	p_keyStore.createStore();

	p_orderTree.setPath(this->getPath());
	p_orderTree.createTree();

	processQueue();
}

void JsView::loadView() {
	p_storage = getEngine()->getStorage(p_storageName);
	
	p_instances.push(new JsInstance(p_path + "/../../extern/" + p_scriptFile));

	p_keyStore.setPath(this->getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_keyStore.createStore();

	p_orderTree.setPath(this->getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_orderTree.createTree();

	processQueue();
}

Proto::ViewConfig JsView::writeConfig() {
	Proto::ViewConfig config;
	config.set_driver("JsView");
	config.set_identifier(getIdentifier());
	config.set_base_storage(p_storageName);
	config.set_script_file(p_scriptFile);
	return config;
}
void JsView::readConfig(const Proto::ViewConfig &config) {
	if(!config.has_base_storage()
			|| !config.has_script_file())
		throw std::runtime_error("Illegal configuration for JsView");
	p_storageName = config.base_storage();
	p_scriptFile = config.script_file();
}

void JsView::processInsert(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
	auto closure = new InsertClosure(this, mutation.documentId,
			sequence_id, mutation.buffer, callback);
	closure->apply();
}

void JsView::processModify(SequenceId sequence_id,
		Mutation &mutation, Async::Callback<void(Error)> callback) {
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

JsView::JsInstance *JsView::grabInstance() {
	if(p_instances.size() == 0) //FIXME
		throw std::runtime_error("No javascript instance available");
	JsInstance *instance = p_instances.top();
	p_instances.pop();
	return instance;
}

void JsView::releaseInstance(JsInstance *instance) {
	p_instances.push(instance);
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
	p_isolate = v8::Isolate::New();

	v8::Locker locker(p_isolate);
	v8::Isolate::Scope isolate_scope(p_isolate);
	v8::HandleScope handle_scope;
	
	// setup the global object and create a context
	v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	v8::Local<v8::External> js_this = v8::External::New(this);
	global->Set(v8::String::New("log"), v8::FunctionTemplate::New(&JsInstance::jsLog, js_this));
	global->Set(v8::String::New("hook"), v8::FunctionTemplate::New(&JsInstance::jsHook, js_this));
	
	p_context = v8::Context::New(NULL, global);

	v8::Context::Scope context_scope(p_context);
	
	// load the view script from a file
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(script_path, Linux::FileMode::read);
	
	Linux::size_type length = file->lengthSync();
	char *buffer = new char[length];
	file->preadSync(0, length, buffer);
	file->closeSync();
	
	v8::Local<v8::String> source = v8::String::New(buffer, length);
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
	args[0] = v8::Number::New(id);
	args[1] = v8::String::New((char*)buffer, length);
	auto result = p_hookExtractDoc->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::extractKey(const void *buffer,
		Linux::size_type length) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = v8::String::New((char*)buffer, length);
	auto result = p_hookExtractKey->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::keyOf(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	auto result = p_hookKeyOf->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::compare(v8::Handle<v8::Value> a,
		v8::Handle<v8::Value> b) {
	std::array<v8::Handle<v8::Value>, 2> args;
	args[0] = a;
	args[1] = b;
	auto result = p_hookCompare->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::serializeKey(v8::Handle<v8::Value> key) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = key;
	auto result = p_hookSerializeKey->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::deserializeKey(v8::Handle<v8::Value> buffer) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = buffer;
	auto result = p_hookDeserializeKey->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Local<v8::Value> JsView::JsInstance::report(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	auto result = p_hookReport->Call(p_context->Global(),
			args.size(), args.data());
	return result;
}

v8::Handle<v8::Value> JsView::JsInstance::jsLog(const v8::Arguments &args) {
	JsInstance *thisptr = (JsInstance*)v8::External::Unwrap(args.Data());
	if(thisptr->p_enableLog) {
		for(int i = 0; i < args.Length(); i++) {
			v8::String::Utf8Value utf8(args[i]);
			printf("%s", *utf8);
		}
		printf("\n");
	}
	return v8::Undefined();
}

v8::Handle<v8::Value> JsView::JsInstance::jsHook(const v8::Arguments &args) {
	if(args.Length() != 2)
		throw std::runtime_error("hook() expects exactly two arguments");
	JsInstance *thisptr = (JsInstance*)v8::External::Unwrap(args.Data());

	auto func = v8::Local<v8::Function>::Cast(args[1]);

	v8::String::Utf8Value hook_utf8(args[0]);
	std::string hook_name(*hook_utf8, hook_utf8.length());
	if(hook_name == "extractDoc") {
		thisptr->p_hookExtractDoc = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "extractKey") {
		thisptr->p_hookExtractKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "keyOf") {
		thisptr->p_hookKeyOf = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "compare") {
		thisptr->p_hookCompare = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "serializeKey") {
		thisptr->p_hookSerializeKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "deserializeKey") {
		thisptr->p_hookDeserializeKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "report") {
		thisptr->p_hookReport = v8::Persistent<v8::Function>::New(func);
	}else throw std::runtime_error("Illegal hook '" + hook_name + "'");
	return v8::Undefined();
}

// --------------------------------------------------------
// JsView::JsScope
// --------------------------------------------------------

JsView::JsScope::JsScope(JsInstance &instance)
	: p_locker(instance.p_isolate), p_isolateScope(instance.p_isolate),
		p_contextScope(instance.p_context), p_handleScope() { }

// --------------------------------------------------------
// JsView::InsertClosure
// --------------------------------------------------------

JsView::InsertClosure::InsertClosure(JsView *view, DocumentId document_id,
		SequenceId sequence_id, std::string buffer,
		Async::Callback<void(Error)> callback)
	: p_view(view), p_documentId(document_id),
		p_sequenceId(sequence_id), p_buffer(buffer),
		p_callback(callback), p_btreeInsert(&view->p_orderTree) {
	p_instance = p_view->grabInstance();
}

void JsView::InsertClosure::apply() {
	JsScope scope(*p_instance);
	
	v8::Local<v8::Value> extracted = p_instance->extractDoc(p_documentId,
		p_buffer.data(), p_buffer.length());
	p_newKey = v8::Persistent<v8::Value>::New(p_instance->keyOf(extracted));

	v8::Local<v8::Value> ser = p_instance->serializeKey(p_newKey);
	v8::String::Utf8Value ser_value(ser);	
	
	/* add the document's key to the key store */
	auto key_object = p_view->p_keyStore.createObject();
	p_view->p_keyStore.allocObject(key_object, ser_value.length());
	p_view->p_keyStore.writeObject(key_object, 0, ser_value.length(), *ser_value);
	p_insertKey = p_view->p_keyStore.objectLid(key_object);

	OS::packLe64(p_linkBuffer + Link::kDocumentId, p_documentId);
	OS::packLe64(p_linkBuffer + Link::kSequenceId, p_sequenceId);

	/* TODO: re-use removed document entries */
	
	p_btreeInsert.insert(&p_insertKey, p_linkBuffer,
		ASYNC_MEMBER(this, &InsertClosure::compareToNew),
		ASYNC_MEMBER(this, &InsertClosure::onComplete));
}
void JsView::InsertClosure::compareToNew(const DocumentId &keyid_a,
		Async::Callback<void(int)> callback) {
	auto object_a = p_view->p_keyStore.getObject(keyid_a);
	auto length_a = p_view->p_keyStore.objectLength(object_a);
	char *buf_a = new char[length_a];
	p_view->p_keyStore.readObject(object_a, 0, length_a, buf_a);

	JsScope scope(*p_instance);
	
	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	delete[] buf_a;
	v8::Local<v8::Value> key_a = p_instance->deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_instance->compare(key_a, p_newKey);
	callback(result->Int32Value());
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
		p_onComplete(on_complete), p_btreeFind(&view->p_orderTree),
		p_btreeIterate(&view->p_orderTree) {
	p_instance = p_view->grabInstance();
}

void JsView::QueryClosure::process() {
	JsScope scope(*p_instance);

	if(p_query->useToKey)
		p_endKey = v8::Persistent<v8::Value>::New(p_instance->extractKey(p_query->toKey.c_str(),
				p_query->toKey.size()));
	
	Btree<DocumentId>::Ref begin_ref;
	if(p_query->useFromKey) {
		p_beginKey = v8::Persistent<v8::Value>::New(p_instance->extractKey(p_query->fromKey.c_str(),
				p_query->fromKey.size()));
		p_btreeFind.findNext(ASYNC_MEMBER(this, &QueryClosure::compareToBegin),
				ASYNC_MEMBER(this, &QueryClosure::onFindBegin));
	}else{
		p_btreeFind.findFirst(ASYNC_MEMBER(this, &QueryClosure::onFindBegin));
	}
}
void JsView::QueryClosure::onFindBegin(Btree<DocumentId>::Ref ref) {
	p_btreeIterate.seek(ref, ASYNC_MEMBER(this, &QueryClosure::fetchItem));
}
void JsView::QueryClosure::compareToBegin(const DocumentId &keyid_a,
		Async::Callback<void(int)> callback) {
	auto object_a = p_view->p_keyStore.getObject(keyid_a);
	auto length_a = p_view->p_keyStore.objectLength(object_a);
	char *buf_a = new char[length_a];
	p_view->p_keyStore.readObject(object_a, 0, length_a, buf_a);
	
	JsScope scope(*p_instance);

	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	delete[] buf_a;
	v8::Local<v8::Value> key_a = p_instance->deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_instance->compare(key_a, p_beginKey);
	callback(result->Int32Value());
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
	p_onComplete(kQuerySuccess);
	delete this;
}

}; // namespace Db

