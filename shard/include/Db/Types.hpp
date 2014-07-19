
namespace Db {

typedef int64_t id_type;
typedef int64_t trid_type;

struct Mutation {
	enum Type {
		kTypeNone, kTypeInsert, kTypeModify
	};

	Type type;
	int storageIndex;
	id_type documentId;
	std::string buffer;
};

}

