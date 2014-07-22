
#include "Async.hpp"

namespace Db {

class Engine;

struct Query {
	int viewIndex;

	bool useFromKey;
	bool useToKey;
	
	std::string fromKey;
	std::string toKey;
	int limit;

	Query() : useFromKey(false), useToKey(false), limit(-1) { }
};

struct QueryData {
	std::vector<std::string> items;
};

class ViewDriver {
public:
	class Factory {
	public:
		inline Factory(const std::string &identifier)
			: p_identifier(identifier) {
		}
		
		virtual ViewDriver *newDriver(Engine *engine) = 0;
		
		inline std::string getIdentifier() {
			return p_identifier;
		}
	protected:
		std::string p_identifier;
	};
	
	ViewDriver(Engine *engine)
			: p_engine(engine) {
	}

	virtual void createView() = 0;
	virtual void loadView() = 0;

	virtual Proto::ViewConfig writeConfig() = 0;
	virtual void readConfig(const Proto::ViewConfig &config) = 0;

	virtual void sequence(std::vector<Mutation> &mutations,
			Async::Callback<void()> callback) = 0;

	virtual void processQuery(Query *request,
			Async::Callback<void(QueryData &)> report,
			Async::Callback<void(Error)> callback) = 0;

	inline Engine *getEngine() {
		return p_engine;
	}

	inline void setIdentifier(const std::string &identifier) {
		p_identifier = identifier;
	}
	inline std::string getIdentifier() {
		return p_identifier;
	}
	
	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}

protected:
	Engine *p_engine;
	std::string p_identifier;
	std::string p_path;
};

class ViewRegistry {
public:
	void addDriver(ViewDriver::Factory *factory) {
		p_drivers.push_back(factory);
	}

	inline ViewDriver::Factory *getDriver
			(const std::string &identifier) {
		for(int i = 0; i < p_drivers.size(); ++i)
			if(p_drivers[i]->getIdentifier() == identifier)
				return p_drivers[i];
		return nullptr;
	}
	
private:
	std::vector<ViewDriver::Factory*> p_drivers;
};

extern ViewRegistry globViewRegistry;

};

