
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

struct Constraint {
	enum Type {
		kTypeNone, kTypeDocumentState
	};

	Type type;
	int storageIndex;
	DocumentId documentId;
	SequenceId sequenceId;
	bool mustExist;
	bool matchSequenceId;
};

class Sequenceable {
public:
	// inspect a mutation that is replayed.
	// the primary use of this function is that storages
	// can make sure that no document id is allocated twice,
	// even if the server crashed
	virtual void reinspect(Mutation &mutation) = 0;

	virtual void sequence(SequenceId sequence_id,
			std::vector<Mutation> &mutations,
			Async::Callback<void()> callback) = 0;
};

}

