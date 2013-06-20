
namespace Db {

class MutatorDriver {
public:
	virtual void update(id_type object,
			std::function<void(const char*, Linux::size_type)> callback) = 0;
};

typedef int64_t trid_type;

enum TransactState {
	kStNone, kStFixSuccess, kStFixFailure, kStCommit, kStRollback, kStCleanup
};

typedef std::function<void(TransactState)> StateCallback;

class TransactManager {
public:
	TransactManager(Engine *engine) : p_engine(engine), p_nextTrid(1) { }
	
	std::tuple<Error, trid_type> transaction(StateCallback callback);
	void submit(trid_type trid, const Proto::Update &update);
	void fix(trid_type trid);
	void commit(trid_type trid);
	void rollback(trid_type trid);
	void cleanup(trid_type trid);
	
private:
	enum InternalState {
		kIsInitial, kIsQueued, kIsFixed
	};

	struct UpdateInfo {
		UpdateInfo(trid_type trid, const Proto::Update &desc)
			: trid(trid), desc(desc) {
		}
		
		trid_type trid;
		Proto::Update desc;
	};
	
	struct TransactInfo {
		TransactInfo(StateCallback callback)
			: state(kIsInitial), callback(callback) {
		}
		
		InternalState state;
		StateCallback callback;
		std::vector<UpdateInfo*> updateStack;
		int fixSuccess;
	};
	
	struct ObjectInfo {
		ObjectInfo() : touched(false), fixState(false) {
		}
		
		bool touched;
		bool fixState;
		char *fixBuffer;
		Linux::size_type fixLength;
		
		std::deque<UpdateInfo*> fixQueue;
	};

	Engine *p_engine;

	trid_type p_nextTrid;	

	std::map<trid_type, TransactInfo*> p_transactMap;
	std::map<id_type, ObjectInfo*> p_objectMap;
	std::vector<id_type> p_touchedStack;
	
	void p_processTouched();
	void p_unfixTransact(trid_type trid);
	
	TransactInfo *p_getTransact(trid_type trid);
	void p_releaseTransact(trid_type trid);
	
	ObjectInfo *p_allocObject(id_type id);
	ObjectInfo *p_getObject(id_type id);
	void p_releaseObject(id_type id);
};

};

