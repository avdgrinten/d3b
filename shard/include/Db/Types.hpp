
namespace Db {

typedef int64_t DocumentId;
typedef int64_t TransactionId;

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

