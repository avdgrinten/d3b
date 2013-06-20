
#include <cassert>
#include <cstring>
#include <v8.h>
#include "Ll/Btree.h"

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
	
	virtual void onInsert(int storage, id_type id,
			const void *document, Linux::size_type length);

	virtual void query(const Proto::Query &request,
			std::function<void(const Proto::Rows &)> report,
			std::function<void()> callback);

private:
	std::string p_scriptFile;
	std::string p_storageName;
	
	int p_storage;
	Btree<id_type> p_orderTree;
	v8::Persistent<v8::ObjectTemplate> p_global;
	v8::Persistent<v8::Context> p_context;

	bool p_enableLog;
	static v8::Handle<v8::Value> p_jsLog(const v8::Arguments &args);
	static v8::Handle<v8::Value> p_jsHook(const v8::Arguments &args);

	v8::Persistent<v8::Function> p_funExtractDoc;
	v8::Persistent<v8::Function> p_funExtractKey;
	v8::Persistent<v8::Function> p_funKeyOf;
	v8::Persistent<v8::Function> p_funCompare;
	v8::Persistent<v8::Function> p_funReport;

	void p_setupJs();

	v8::Local<v8::Value> p_extractDoc(id_type id,
			const void *buffer, Linux::size_type length);
	v8::Local<v8::Value> p_extractKey(const void *buffer,
			Linux::size_type length);
	v8::Local<v8::Value> p_keyOf(v8::Handle<v8::Value> document);
	v8::Local<v8::Value> p_compare(v8::Handle<v8::Value> a,
			v8::Handle<v8::Value> b);
	v8::Local<v8::Value> p_report(v8::Handle<v8::Value> document);
};

}

