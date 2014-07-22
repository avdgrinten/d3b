
namespace Db {

typedef int64_t DocumentId;
typedef int64_t TransactionId;
typedef int64_t SequenceId;

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

