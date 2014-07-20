
namespace Db {

typedef int64_t DocumentId;
typedef int64_t trid_type;

struct Mutation {
	enum Type {
		kTypeNone, kTypeInsert, kTypeModify
	};

	Type type;
	int storageIndex;
	DocumentId documentId;
	std::string buffer;
};

}

