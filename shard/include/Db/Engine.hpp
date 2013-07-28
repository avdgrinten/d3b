
#include <cstdint>
#include <string>
#include <vector>

namespace Db {

typedef int64_t id_type;

class Engine {
public:
	Engine();
	
	void createConfig();
	void loadConfig();

	void createStorage(const Proto::StorageConfig &config);
	int getStorage(const std::string &identifier);
	void unlinkStorage(int storage);

	void createView(const Proto::ViewConfig &config);
	int getView(const std::string &view);
	void unlinkView(int view);

	/* transaction interface */
	std::tuple<Error, trid_type> transaction
			(std::function<void(TransactState)> callback);
	Error submit(trid_type trid, const Proto::Update &update);
	Error fix(trid_type trid);
	Error commit(trid_type trid);
	Error rollback(trid_type trid);
	Error cleanup(trid_type trid);

	Error allocId(int storage, id_type *out_id);
	Error insert(int storage, id_type id,
			const void *document, Linux::size_type length);
	Error update(int storage, id_type id,
			const void *document, Linux::size_type length);
	Linux::size_type length(int storage, id_type id);
	void fetch(int storage, id_type id, void *buffer);
	
	Error query(const Proto::Query &request,
			std::function<void(const Proto::Rows&)> report,
			std::function<void()> callback);
	
	inline void setPath(const std::string &path) {
		p_path = path;
	}
	inline std::string getPath() {
		return p_path;
	}
	
private:
	TransactManager *p_transactManager;
	
	std::string p_path;
	std::vector<StorageDriver*> p_storage;
	std::vector<ViewDriver*> p_views;

	StorageDriver *p_setupStorage(const Proto::StorageConfig &config);
	ViewDriver *p_setupView(const Proto::ViewConfig &config);
	
	void p_writeConfig();
};

};

