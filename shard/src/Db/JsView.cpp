

#include <cstdint>
#include <array>
#include <string>
#include <iostream>

#include "Os/Linux.h"
#include "Async.h"
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
		p_orderTree("order", 4096, sizeof(id_type), sizeof(id_type)) {
	v8::HandleScope handle_scope;
	
	v8::Local<v8::External> js_this = v8::External::New(this);
	
	p_global = v8::Persistent<v8::ObjectTemplate>::New
			(v8::ObjectTemplate::New());
	p_global->Set(v8::String::New("log"),
			v8::FunctionTemplate::New(JsView::p_jsLog, js_this));
	p_global->Set(v8::String::New("hook"),
			v8::FunctionTemplate::New(JsView::p_jsHook, js_this));
	
	p_context = v8::Context::New(NULL, p_global);
	
	p_orderTree.setCompare([this] (id_type keyid_a, id_type keyid_b) -> int {
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
	});
	p_orderTree.setWriteKey([] (void *buffer, id_type id) {
		*(id_type*)buffer = OS::toLe(id);
	});
	p_orderTree.setReadKey([] (const void *buffer) -> id_type {
		return OS::fromLe(*(id_type*)buffer);
	});
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
	p_keyStore.loadStore();

	p_orderTree.setPath(this->getPath());
	p_orderTree.loadTree();
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

void JsView::processUpdate(Mutation *mutation,
		std::function<void(Error)> callback) {
	if(mutation->type == Mutation::kTypeInsert) {
		p_onInsert(mutation->documentId, mutation->buffer.data(),
			mutation->buffer.size(), callback);
	}else if(mutation->type == Mutation::kTypeModify) {
		Async::staticSeries(std::make_tuple(
			[=](std::function<void(Error)> ser_cb) {
				p_onRemove(mutation->documentId, ser_cb);
			},
			[=](std::function<void(Error)> ser_cb) {
				p_onInsert(mutation->documentId, mutation->buffer.data(),
					mutation->buffer.size(), ser_cb);
			}
		), callback);
	}else throw std::logic_error("Illegal mutation type");
}

void JsView::p_onInsert(id_type id,
		const void *document, Linux::size_type length,
		std::function<void(Error)> callback) {
	v8::Context::Scope context_scope(p_context);
	v8::HandleScope handle_scope;
	
	v8::Local<v8::Value> extracted = p_extractDoc(id, document, length);
	auto key = v8::Persistent<v8::Value>::New(p_keyOf(extracted));
	v8::Local<v8::Value> ser = p_serializeKey(key);

	v8::String::Utf8Value ser_value(ser);	
	
	/* add the document's key to the key store */
	auto key_object = p_keyStore.createObject();
	p_keyStore.allocObject(key_object, ser_value.length());
	p_keyStore.writeObject(key_object, 0, ser_value.length(), *ser_value);
	id_type key_id = p_keyStore.objectLid(key_object);

	id_type written_id = OS::toLe(id);

	/* TODO: re-use removed document entries */
	
	p_orderTree.insert(key_id, &written_id, [this, key] (id_type keyid_a) -> int {
		auto object_a = p_keyStore.getObject(keyid_a);
		auto length_a = p_keyStore.objectLength(object_a);
		char *buf_a = new char[length_a];
		p_keyStore.readObject(object_a, 0, length_a, buf_a);
	
		v8::Context::Scope context_scope(p_context);
		v8::HandleScope handle_scope;
		
		v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
		delete[] buf_a;
		v8::Local<v8::Value> key_a = p_deserializeKey(ser_a);
		v8::Local<v8::Value> result = p_compare(key_a, key);
		return result->Int32Value();
	});
	callback(Error(true));
}
void JsView::p_onRemove(id_type id,
		std::function<void(Error)> callback) {
	FetchRequest *fetch = new FetchRequest;
	fetch->storageIndex = p_storage;
	fetch->documentId = id;
	
	getEngine()->fetch(fetch, [=](FetchData &data) {
		v8::Context::Scope context_scope(p_context);
		v8::HandleScope handle_scope;
		
		auto extracted = v8::Persistent<v8::Value>::New(p_extractDoc(id,
				data.buffer.data(), data.buffer.size()));
		auto key = v8::Persistent<v8::Value>::New(p_keyOf(extracted));
		
		Btree<id_type>::Ref ref = p_orderTree.findNext
				([this, key] (id_type keyid_a) -> int {
			auto object_a = p_keyStore.getObject(keyid_a);
			auto length_a = p_keyStore.objectLength(object_a);
			char *buf_a = new char[length_a];
			p_keyStore.readObject(object_a, 0, length_a, buf_a);
			
			v8::Context::Scope context_scope(p_context);
			v8::HandleScope handle_scope;
			
			v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
			delete[] buf_a;
			v8::Local<v8::Value> key_a = p_deserializeKey(ser_a);
			v8::Local<v8::Value> result = p_compare(key_a, key);
			return result->Int32Value();
		}, nullptr, nullptr);
		
		Btree<id_type>::Seq seq = p_orderTree.sequence(ref);

		/* advance the sequence position until we reach
				the specified document id */
		while(true) {
			if(!seq.valid())
				throw std::runtime_error("Specified document not in tree");
			
			id_type read_id;
			seq.getValue(&read_id);
			id_type cur_id = OS::fromLe(read_id);
			
			if(cur_id == id)
				break;
			++seq;
		}
		
		// set the id to zero to remove the document
		id_type write_id = 0;
		seq.setValue(&write_id);
	}, [=](Error error) {
		delete fetch;
		callback(error);
	});
}

void JsView::processQuery(Proto::Query *request,
		std::function<void(QueryData &)> report,
		std::function<void(Error)> callback) {	
	v8::Context::Scope context_scope(p_context);
	v8::HandleScope handle_scope;

	v8::Persistent<v8::Value> end_key;
	if(request->has_to_key())
		end_key = v8::Persistent<v8::Value>::New(p_extractKey(request->to_key().c_str(),
				request->to_key().size()));
	
	Btree<id_type>::Ref begin_ref;
	if(request->has_from_key()) {
		auto begin_key = v8::Persistent<v8::Value>::New(p_extractKey(request->from_key().c_str(),
				request->from_key().size()));
		begin_ref = p_orderTree.findNext([this, begin_key] (id_type keyid_a) -> int {
			auto object_a = p_keyStore.getObject(keyid_a);
			auto length_a = p_keyStore.objectLength(object_a);
			char *buf_a = new char[length_a];
			p_keyStore.readObject(object_a, 0, length_a, buf_a);
			
			v8::Context::Scope context_scope(p_context);
			v8::HandleScope handle_scope;

			v8::Local<v8::Value> ser_a = v8::String::New(buf_a, length_a);
			delete[] buf_a;
			v8::Local<v8::Value> key_a = p_deserializeKey(ser_a);
			v8::Local<v8::Value> result = p_compare(key_a, begin_key);
			return result->Int32Value();
		}, nullptr, nullptr);
	}else{
		begin_ref = p_orderTree.refToFirst();
	}

	struct control_struct {
		Btree<id_type>::Seq sequence;
		FetchRequest fetch;
		QueryData rows;
		int count;
		
		control_struct(Btree<id_type>::Seq &&sequence)
				: sequence(std::move(sequence)) { }
	};
	control_struct *control = new control_struct(
		p_orderTree.sequence(begin_ref));
	control->count = 0;
	
	Async::whilst([request, control]() -> bool {
		if(!control->sequence.valid())
			return false;
		if(request->has_limit() && control->count == request->limit())
			return false;
		return true;
	}, [this, report, control](std::function<void(Error)> elem_cb) {
		id_type read_id;
		control->sequence.getValue(&read_id);
		id_type id = OS::fromLe(read_id);

		// skip removed documents
		if(id == 0) {
			++control->sequence;
			return elem_cb(Error(true));
		}
		
		control->fetch.storageIndex = p_storage;
		control->fetch.documentId = id;
		getEngine()->fetch(&control->fetch, [this, id, control](FetchData &data) {
			v8::Context::Scope context_scope(p_context);
			v8::HandleScope handle_scope;
			
			v8::Local<v8::Value> extracted = p_extractDoc(id,
					data.buffer.data(), data.buffer.length());

			/* FIXME:
			if(!end_key.IsEmpty()) {
				v8::Local<v8::Value> key = p_keyOf(extracted);
				if(p_compare(key, end_key)->Int32Value() > 0)
					break;
			}*/
			
			v8::Local<v8::Value> rpt_value = p_report(extracted);
			v8::String::Utf8Value rpt_utf8(rpt_value);
			control->rows.items.emplace_back(*rpt_utf8, rpt_utf8.length());
		}, [report, control, elem_cb](Error error) {
			if(!error.ok())
				return elem_cb(error);

			if(control->rows.items.size() >= 1000) {
				report(control->rows);
				control->rows.items.clear();
			}
			++control->sequence;
			++control->count;
			osIntf->nextTick([elem_cb]() { elem_cb(Error(true)); });
		});
	}, [=] (Error error) {
		if(!error.ok())
			return callback(error);
		if(control->rows.items.size() > 0)
			report(control->rows);
		delete control;
		callback(Error(true));
	});
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

v8::Local<v8::Value> JsView::p_extractDoc(id_type id,
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

