

#include <cstdint>
#include <array>
#include <string>
#include <iostream>

#include "Os/Linux.hpp"
#include "Async.hpp"
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

v8::Handle<v8::Value> JsView::p_jsLog(const v8::Arguments &args) {
	JsView *thisptr = (JsView*)v8::External::Unwrap(args.Data());
	if(thisptr->p_enableLog) {
		for(int i = 0; i < args.Length(); i++) {
			v8::String::Utf8Value utf8(args[i]);
			printf("%s", *utf8);
		}
		printf("\n");
	}
	return v8::Undefined();
}

v8::Handle<v8::Value> JsView::p_jsHook(const v8::Arguments &args) {
	if(args.Length() != 2)
		throw std::runtime_error("register() expects exactly two arguments");
	JsView *thisptr = (JsView*)v8::External::Unwrap(args.Data());

	auto func = v8::Local<v8::Function>::Cast(args[1]);

	v8::String::Utf8Value hook_utf8(args[0]);
	std::string hook_name(*hook_utf8, hook_utf8.length());
	if(hook_name == "extractDoc") {
		thisptr->p_funExtractDoc = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "extractKey") {
		thisptr->p_funExtractKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "keyOf") {
		thisptr->p_funKeyOf = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "compare") {
		thisptr->p_funCompare = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "serializeKey") {
		thisptr->p_funSerializeKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "deserializeKey") {
		thisptr->p_funDeserializeKey = v8::Persistent<v8::Function>::New(func);
	}else if(hook_name == "report") {
		thisptr->p_funReport = v8::Persistent<v8::Function>::New(func);
	}else throw std::runtime_error("Illegal hook '" + hook_name + "'");
	return v8::Undefined();
}

JsView::JsView(Engine *engine) : ViewDriver(engine),
		p_enableLog(true), p_keyStore("keys"),
		p_orderTree("order", 4096, sizeof(DocumentId), sizeof(DocumentId)) {
	v8::HandleScope handle_scope;
	
	v8::Local<v8::External> js_this = v8::External::New(this);
	
	p_global = v8::Persistent<v8::ObjectTemplate>::New
			(v8::ObjectTemplate::New());
	p_global->Set(v8::String::New("log"),
			v8::FunctionTemplate::New(JsView::p_jsLog, js_this));
	p_global->Set(v8::String::New("hook"),
			v8::FunctionTemplate::New(JsView::p_jsHook, js_this));
	
	p_context = v8::Context::New(NULL, p_global);
	
	p_orderTree.setCompare(ASYNC_MEMBER(this, &JsView::compare));
	p_orderTree.setWriteKey(ASYNC_MEMBER(this, &JsView::writeKey));
	p_orderTree.setReadKey(ASYNC_MEMBER(this, &JsView::readKey));
}

int JsView::compare(const DocumentId &keyid_a, const DocumentId &keyid_b) {
	auto object_a = p_keyStore.getObject(keyid_a);
	auto object_b = p_keyStore.getObject(keyid_b);

	auto length_a = p_keyStore.objectLength(object_a);
	auto length_b = p_keyStore.objectLength(object_b);
	char *buf_a = new char[length_a];
	char *buf_b = new char[length_b];
	p_keyStore.readObject(object_a, 0, length_a, buf_a);
	p_keyStore.readObject(object_b, 0, length_b, buf_b);
	
	v8::Context::Scope context_scope(p_context);
	v8::HandleScope handle_scope;
	v8::TryCatch trycatch;

	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	v8::Local<v8::Value> ser_b = v8::String::New(buf_b, length_b);
	delete[] buf_a;
	delete[] buf_b;

	v8::Local<v8::Value> key_a = p_deserializeKey(ser_a);
	v8::Local<v8::Value> key_b = p_deserializeKey(ser_b);
	v8::Local<v8::Value> result = p_compare(key_a, key_b);
	if(trycatch.HasCaught())
		throw std::runtime_error("Unexpected javascript exception");
	return result->Int32Value();
};
void JsView::writeKey(void *buffer, const DocumentId &key) {
	*(DocumentId*)buffer = OS::toLe(key);
}
DocumentId JsView::readKey(const void *buffer) {
	return OS::fromLe(*(DocumentId*)buffer);
}

void JsView::createView() {
	p_setupJs();
	
	p_keyStore.setPath(this->getPath());
	p_keyStore.createStore();

	p_orderTree.setPath(this->getPath());
	p_orderTree.createTree();
}

void JsView::loadView() {
	p_setupJs();

	p_keyStore.setPath(this->getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_keyStore.createStore();

	p_orderTree.setPath(this->getPath());
	//NOTE: to test the durability implementation we always delete the data on load!
	p_orderTree.createTree();
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

JsView::SequenceClosure::SequenceClosure(JsView *view, std::vector<Mutation> &mutations,
		Async::Callback<void()> callback)
	: p_view(view), p_mutations(mutations), p_callback(callback), p_index(0) { }

void JsView::SequenceClosure::apply() {
	if(p_index == p_mutations.size()) {
		complete();
		return;
	}
	
	Mutation &mutation = p_mutations[p_index];
	if(mutation.type == Mutation::kTypeInsert) {
		auto closure = new InsertClosure(p_view,
				mutation.documentId, mutation.buffer,
				ASYNC_MEMBER(this, &SequenceClosure::insertOnComplete));
		closure->apply();
	}else if(mutation.type == Mutation::kTypeModify) {
		auto closure = new RemoveClosure(p_view, mutation.documentId,
			ASYNC_MEMBER(this, &SequenceClosure::modifyOnRemove));
		closure->apply();
	}else throw std::logic_error("Illegal mutation type");
}
void JsView::SequenceClosure::insertOnComplete(Error error) {
	//FIXME: don't ignore error
	p_index++;
	apply();
}
void JsView::SequenceClosure::modifyOnRemove(Error error) {
	//FIXME: don't ignore error
	Mutation &mutation = p_mutations[p_index];
	auto closure = new InsertClosure(p_view,
			mutation.documentId, mutation.buffer,
			ASYNC_MEMBER(this, &SequenceClosure::modifyOnInsert));
	closure->apply();
}
void JsView::SequenceClosure::modifyOnInsert(Error error) {
	//FIXME: don't ignore error
	p_index++;
	apply();
}
void JsView::SequenceClosure::complete() {
	p_callback();
	delete this;
}

void JsView::sequence(std::vector<Mutation> &mutations,
		Async::Callback<void()> callback) {
	auto closure = new SequenceClosure(this, mutations, callback);
	closure->apply();
}

JsView::InsertClosure::InsertClosure(JsView *view, DocumentId document_id,
		std::string buffer, Async::Callback<void(Error)> callback)
	: p_view(view), p_documentId(document_id), p_buffer(buffer),
		p_callback(callback), p_btreeInsert(&view->p_orderTree) { }

void JsView::InsertClosure::apply() {
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> extracted = p_view->p_extractDoc(p_documentId,
		p_buffer.data(), p_buffer.length());
	p_newKey = v8::Persistent<v8::Value>::New(p_view->p_keyOf(extracted));

	v8::Local<v8::Value> ser = p_view->p_serializeKey(p_newKey);
	v8::String::Utf8Value ser_value(ser);	
	
	/* add the document's key to the key store */
	auto key_object = p_view->p_keyStore.createObject();
	p_view->p_keyStore.allocObject(key_object, ser_value.length());
	p_view->p_keyStore.writeObject(key_object, 0, ser_value.length(), *ser_value);
	p_insertKey = p_view->p_keyStore.objectLid(key_object);

	DocumentId written_id = OS::toLe(p_documentId);

	/* TODO: re-use removed document entries */
	
	p_btreeInsert.insert(&p_insertKey, &written_id,
		ASYNC_MEMBER(this, &InsertClosure::compareToNew),
		ASYNC_MEMBER(this, &InsertClosure::onComplete));
}
void JsView::InsertClosure::compareToNew(const DocumentId &keyid_a,
		Async::Callback<void(int)> callback) {
	auto object_a = p_view->p_keyStore.getObject(keyid_a);
	auto length_a = p_view->p_keyStore.objectLength(object_a);
	char *buf_a = new char[length_a];
	p_view->p_keyStore.readObject(object_a, 0, length_a, buf_a);

	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	delete[] buf_a;
	v8::Local<v8::Value> key_a = p_view->p_deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_view->p_compare(key_a, p_newKey);
	callback(result->Int32Value());
}
void JsView::InsertClosure::onComplete() {
	p_callback(Error(true));
	delete this;
}

JsView::RemoveClosure::RemoveClosure(JsView *view, DocumentId document_id,
		Async::Callback<void(Error)> callback)
	: p_view(view), p_documentId(document_id), p_callback(callback),
		p_btreeFind(&view->p_orderTree), p_btreeIterate(&view->p_orderTree) { }

void JsView::RemoveClosure::apply() {
	p_fetch.storageIndex = p_view->p_storage;
	p_fetch.documentId = p_documentId;
	
	p_view->getEngine()->fetch(&p_fetch,
		ASYNC_MEMBER(this, &RemoveClosure::onFetchData),
		ASYNC_MEMBER(this, &RemoveClosure::onFetchComplete));
}
void JsView::RemoveClosure::onFetchComplete(Error error) {
	//FIXME: don't ignore error
	p_callback(Error(true));
	delete(this);
}
void JsView::RemoveClosure::onFetchData(FetchData &data) {
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> extracted = p_view->p_extractDoc(p_documentId,
			data.buffer.data(), data.buffer.size());
	p_removedKey = v8::Persistent<v8::Value>::New(p_view->p_keyOf(extracted));
		
	p_btreeFind.findNext(ASYNC_MEMBER(this, &RemoveClosure::compareToRemoved),
			ASYNC_MEMBER(this, &RemoveClosure::onFindRemoved));
}
void JsView::RemoveClosure::compareToRemoved(const DocumentId &keyid_a,
		Async::Callback<void(int)> callback) {
	auto object_a = p_view->p_keyStore.getObject(keyid_a);
	auto length_a = p_view->p_keyStore.objectLength(object_a);
	char *buf_a = new char[length_a];
	p_view->p_keyStore.readObject(object_a, 0, length_a, buf_a);
	
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	delete[] buf_a;
	v8::Local<v8::Value> key_a = p_view->p_deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_view->p_compare(key_a, p_removedKey);
	callback(result->Int32Value());
}
void JsView::RemoveClosure::onFindRemoved(Btree<DocumentId>::Ref ref) {
	p_btreeIterate.seek(ref, ASYNC_MEMBER(this, &RemoveClosure::processItem));
}
void JsView::RemoveClosure::processItem() {
	/* advance the sequence position until we reach
			the specified document id */
	if(!p_btreeIterate.valid())
		throw std::runtime_error("Specified document not in tree");
		
	DocumentId read_id;
	p_btreeIterate.getValue(&read_id);
	DocumentId cur_id = OS::fromLe(read_id);
		
	if(cur_id == p_documentId) {
		// set the id to zero to remove the document
		DocumentId write_id = 0;
		p_btreeIterate.setValue(&write_id);
	}else{
		//TODO: use nextTick() to prevent buffer overflow
		p_btreeIterate.forward(ASYNC_MEMBER(this, &RemoveClosure::processItem));
	}
}

JsView::QueryClosure::QueryClosure(JsView *view, Query *query,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(Error)> on_complete)
	: p_view(view), p_query(query), p_onData(on_data),
		p_onComplete(on_complete), p_btreeFind(&view->p_orderTree),
		p_btreeIterate(&view->p_orderTree) { }

void JsView::QueryClosure::process() {
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;

	if(p_query->useToKey)
		p_endKey = v8::Persistent<v8::Value>::New(p_view->p_extractKey(p_query->toKey.c_str(),
				p_query->toKey.size()));
	
	Btree<DocumentId>::Ref begin_ref;
	if(p_query->useFromKey) {
		p_beginKey = v8::Persistent<v8::Value>::New(p_view->p_extractKey(p_query->fromKey.c_str(),
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
	
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;

	v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
	delete[] buf_a;
	v8::Local<v8::Value> key_a = p_view->p_deserializeKey(ser_a);
	v8::Local<v8::Value> result = p_view->p_compare(key_a, p_beginKey);
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

	DocumentId read_id;
	p_btreeIterate.getValue(&read_id);
	DocumentId id = OS::fromLe(read_id);

	// skip removed documents
	if(id == 0) {
		p_btreeIterate.forward(ASYNC_MEMBER(this, &QueryClosure::fetchItemLoop));
		return;
	}
	
	p_fetch.storageIndex = p_view->p_storage;
	p_fetch.documentId = id;
	p_view->getEngine()->fetch(&p_fetch,
			ASYNC_MEMBER(this, &QueryClosure::onFetchData),
			ASYNC_MEMBER(this, &QueryClosure::onFetchComplete));
}
void JsView::QueryClosure::fetchItemLoop() {
	osIntf->nextTick(ASYNC_MEMBER(this, &QueryClosure::fetchItem));
}
void JsView::QueryClosure::onFetchData(FetchData &data) {
	v8::Context::Scope context_scope(p_view->p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> extracted = p_view->p_extractDoc(data.documentId,
			data.buffer.data(), data.buffer.length());

	/* FIXME:
	if(!p_endKey.IsEmpty()) {
		v8::Local<v8::Value> key = p_view->p_keyOf(extracted);
		if(p_compare(key, p_endKey)->Int32Value() > 0)
			break;
	}*/
	
	v8::Local<v8::Value> rpt_value = p_view->p_report(extracted);
	v8::String::Utf8Value rpt_utf8(rpt_value);
	p_queryData.items.emplace_back(*rpt_utf8, rpt_utf8.length());
}
void JsView::QueryClosure::onFetchComplete(Error error) {
	//FIXME: don't ignore error values

	if(p_queryData.items.size() >= 1000) {
		p_onData(p_queryData);
		p_queryData.items.clear();
	}

	++p_fetchedCount;
	p_btreeIterate.forward(ASYNC_MEMBER(this, &QueryClosure::fetchItemLoop));
}
void JsView::QueryClosure::complete() {
	if(p_queryData.items.size() > 0)
		p_onData(p_queryData);

	p_onComplete(Error(true));
	delete this;
}

void JsView::processQuery(Query *request,
		Async::Callback<void(QueryData &)> on_data,
		Async::Callback<void(Error)> on_complete) {
	auto closure = new QueryClosure(this, request, on_data, on_complete);
	closure->process();
}

void JsView::p_setupJs() {
	std::unique_ptr<Linux::File> file = osIntf->createFile();
	file->openSync(p_path + "/../../extern/" + p_scriptFile,
			Linux::FileMode::read);
	
	Linux::size_type length = file->lengthSync();
	char *buffer = new char[length];
	file->preadSync(0, length, buffer);
	file->closeSync();
	
	v8::Context::Scope context_scope(p_context);
	v8::HandleScope handle_scope;

	v8::Local<v8::String> source = v8::String::New(buffer, length);
	delete[] buffer;

	v8::TryCatch except;
	v8::Local<v8::Script> script = v8::Script::Compile(source);
	if(except.HasCaught())
		throw std::runtime_error(std::string("Exception while compiling script:\n")
				+ exceptString(except.Message()));
	script->Run();
	if(except.HasCaught())
		throw std::runtime_error(std::string("Exception while executing script:\n")
				+ exceptString(except.Message()));
	
	p_storage = getEngine()->getStorage(p_storageName);
}

v8::Local<v8::Value> JsView::p_extractDoc(DocumentId id,
		const void *buffer, Linux::size_type length) {
	std::array<v8::Handle<v8::Value>, 2> args;
	args[0] = v8::Number::New(id);
	args[1] = v8::String::New((char*)buffer, length);
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funExtractDoc->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_extractKey(const void *buffer,
		Linux::size_type length) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = v8::String::New((char*)buffer, length);
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funExtractKey->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_keyOf(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funKeyOf->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_compare(v8::Handle<v8::Value> a,
		v8::Handle<v8::Value> b) {
	std::array<v8::Handle<v8::Value>, 2> args;
	args[0] = a;
	args[1] = b;
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funCompare->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_serializeKey(v8::Handle<v8::Value> key) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = key;
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funSerializeKey->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_deserializeKey(v8::Handle<v8::Value> buffer) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = buffer;
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funDeserializeKey->Call(global,
			args.size(), args.data());
	
	return result;
}

v8::Local<v8::Value> JsView::p_report(v8::Handle<v8::Value> document) {
	std::array<v8::Handle<v8::Value>, 1> args;
	args[0] = document;
	
	v8::Local<v8::Object> global = p_context->Global();
	v8::Local<v8::Value> result = p_funReport->Call(global,
			args.size(), args.data());
	
	return result;
}

JsView::Factory::Factory()
		: ViewDriver::Factory("JsView") {
}

ViewDriver *JsView::Factory::newDriver(Engine *engine) {
	return new JsView(engine);
}

};

