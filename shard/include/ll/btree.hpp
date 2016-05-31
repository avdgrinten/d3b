
#ifndef D3B_LL_BTREE_HPP
#define D3B_LL_BTREE_HPP

#include <cstring>

#include <libchain/all.hpp>

#include "ll/page-cache.hpp"

template<typename KeyType>
class Btree {
public:
	typedef int32_t BlkIndexType;

	typedef Async::Callback<void(void *, const KeyType &)> WriteKeyCallback;
	typedef Async::Callback<KeyType(const void*)> ReadKeyCallback;
	typedef Async::Callback<void(const KeyType &, Async::Callback<void(int)>)> UnaryCompareCallback;
	typedef Async::Callback<int(const KeyType &, const KeyType &)> BinaryCompareCallback;

	Btree(std::string name,
			size_t block_size,
			size_t key_size,
			size_t val_size,
			CacheHost *cache_host,
			TaskPool *io_pool);

	class Ref {
	friend class Btree;
	public:
		Ref() : block(-1), entry(-1) {
		}
		
		bool valid() const {
			return block >= 0 && entry >= 0;
		}
		
		bool operator== (const Ref &other) const {
			if(block != other.block)
				return false;
			if(entry != other.entry)
				return false;
			return true;
		}
		
	private:
		Ref(BlkIndexType block, BlkIndexType entry)
				: block(block), entry(entry) {
		}
		
		BlkIndexType block;
		BlkIndexType entry;
	};

	// FIXME: IterateClosure should properly deallocate memory
	class IterateClosure {
	public:
		IterateClosure(Btree *tree) : p_tree(tree), p_block(0), p_entry(0),
				p_buffer(nullptr) { }
		~IterateClosure();
		
		Ref position();
		void seek(Ref ref, Async::Callback<void()> callback);
		void forward(Async::Callback<void()> callback);
		
		bool valid() const {
			return p_block > 0 && p_entry >= 0;
		}
		
		KeyType getKey();
		void getValue(void *value);
		void setValue(void *value);
		
	private:
		void seekOnRead(char *buffer);
		void forwardOnRead(char *buffer);

		Btree *p_tree;
		Async::Callback<void()> p_callback;

		BlkIndexType p_block;
		BlkIndexType p_entry;
		char *p_buffer;
	};

	class SearchNodeClosure {
	public:
		SearchNodeClosure(Btree *tree) : p_tree(tree) { }

		void prevInLeaf(char *block,
				UnaryCompareCallback compare,
				Async::Callback<void(int)> callback);
		void prevInInner(char *block,
				UnaryCompareCallback compare,
				Async::Callback<void(int)> callback);
		void nextInLeaf(char *block,
				UnaryCompareCallback compare,
				Async::Callback<void(int)> callback);
		void nextInInner(char *block,
				UnaryCompareCallback compare,
				Async::Callback<void(int)> callback);
	
	private:
		void prevInLeafLoop();
		void prevInLeafCheck(int result);
		void prevInInnerLoop();
		void prevInInnerCheck(int result);
		void nextInLeafLoop();
		void nextInLeafCheck(int result);
		void nextInInnerLoop();
		void nextInInnerCheck(int result);

		Btree *p_tree;
		UnaryCompareCallback p_compare;
		Async::Callback<void(int)> p_callback;
		
		BlkIndexType p_entryCount;
		BlkIndexType p_entryIndex;
		char *p_blockBuffer;
	};

	class SplitClosure {
	public:
		SplitClosure(Btree *tree) : p_tree(tree) { }

		void split(BlkIndexType block_num, char *block_buf,
				char *parent_buf, BlkIndexType block_index,
				Async::Callback<void()> on_complete);
	
	private:
		void splitLeaf();
		void splitLeafOnInitialize(char *child_buffer);
		void onReadRightLink(char *link_buffer);
		void splitInner();
		void splitInnerOnInitialize(char *child_buffer);
		void fixParent();
		void fixParentOnInitialize(char *new_root_buffer);

		Btree *p_tree;
		Async::Callback<void()> p_onComplete;
		
		BlkIndexType p_blockNumber;
		BlkIndexType p_blockIndex;
		BlkIndexType p_leftSize;
		BlkIndexType p_rightSize;
		BlkIndexType p_splitNumber;
		BlkIndexType p_rightLinkNumber;
		BlkIndexType p_newRootNumber;
		char *p_blockBuffer;
		char *p_parentBuffer;
		KeyType p_splitKey;
	};

	template<typename UnaryCompare>
	auto insert(KeyType *key, void *value, UnaryCompare compare) {
		struct Context {
			Context(Btree *self, KeyType *key, void *value, UnaryCompare compare)
			: self(self), key(key), value(value), compare(compare),
					parentNumber(-1), parentBuffer(nullptr), indexInParent(0),
					splitClosure(self), searchClosure(self) { }

			Btree *self;
			KeyType *key;
			void *value;
			UnaryCompare compare;

			BlkIndexType parentNumber;
			char *parentBuffer;
		
			BlkIndexType indexInParent;
			BlkIndexType currentNumber;
			char *currentBuffer;

			SplitClosure splitClosure;
			SearchNodeClosure searchClosure;
		};

		return libchain::contextify([] (auto c) {
			auto read_block = libchain::sequence()
			+ libchain::apply([c] () -> bool {
				return c->parentNumber == -1;
			})
			+ libchain::branch(
				// we are at the root of the tree
				libchain::apply([c] () {
					c->currentNumber = c->self->p_curFileHead.rootBlock;
				}),

				// determine the indexInParent and read the block number
				libchain::sequence()
				+ libchain::await<void(int)>([c] (auto callback) {
					assert(!(c->self->p_headGetFlags(c->parentBuffer) & BlockHead::kFlagIsLeaf));
					
					c->searchClosure.prevInInner(c->parentBuffer, c->compare,
							Async::transition(callback));
				})
				+ libchain::apply([c] (int index) {
					c->indexInParent = index;

					char *ref_ptr = c->parentBuffer + (c->indexInParent >= 0
							? c->self->p_refOffInner(c->indexInParent)
							: c->self->p_lrefOffInner());
					
					c->currentNumber = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
				})
			)
			+ libchain::await<void(char *)>([c] (auto callback) {
				c->self->p_pageCache.readPage(c->currentNumber,
						Async::transition(callback));
			})
			+ libchain::apply([c] (char *buffer) {
				c->currentBuffer = buffer;
			});

			auto maybe_split_block = libchain::sequence()
			+ read_block
			+ libchain::apply([c] () -> bool {
				return c->self->blockIsFull(c->currentBuffer);
			})
			+ libchain::branch(
				// the current block is full: split it
				libchain::sequence()
				+ libchain::await<void()>([c] (auto callback) {
					c->splitClosure.split(c->currentNumber, c->currentBuffer,
							c->parentBuffer, c->indexInParent,
							Async::transition(callback));
				})
				+ libchain::apply([c] () {
					if(c->parentNumber != -1)
						c->self->p_pageCache.writePage(c->parentNumber);
					c->self->p_pageCache.writePage(c->currentNumber);
					c->self->p_pageCache.releasePage(c->currentNumber);
				})
				// the correct block might have changed; re-read it
				+ read_block
				+ libchain::apply([c] () {
					assert(!c->self->blockIsFull(c->currentBuffer));
				}),

				libchain::apply([c] () { })
			);

			auto insert_or_descend = libchain::sequence()
			+ maybe_split_block
			+ libchain::apply([c] () -> bool {
				int flags = c->self->p_headGetFlags(c->currentBuffer);
				// TODO: remove those assertions
				if(flags & BlockHead::kFlagIsLeaf) {
					assert(c->self->p_leafGetEntCount(c->currentBuffer)
							< c->self->p_entsPerLeaf());
				}else{
					assert(c->self->p_innerGetEntCount(c->currentBuffer)
							< c->self->p_entsPerInner());
				}
				return flags & BlockHead::kFlagIsLeaf;
			})
			+ libchain::branch(
				// it is a leaf: insert an entry here
				libchain::sequence()
				+ libchain::await<void(int)>([c] (auto callback) {
					c->searchClosure.prevInLeaf(c->currentBuffer, c->compare,
							Async::transition(callback));
				})
				+ libchain::apply([c] (int index) -> bool {
					if(index >= 0) {
						c->self->p_insertAtLeaf(c->currentBuffer, index + 1, *c->key, c->value);
					}else{
						c->self->p_insertAtLeaf(c->currentBuffer, 0, *c->key, c->value);
					}
					if(c->parentNumber != -1)
						c->self->p_pageCache.releasePage(c->parentNumber);
					c->self->p_pageCache.writePage(c->currentNumber);
					c->self->p_pageCache.releasePage(c->currentNumber);
					
					return false;
				}),

				// it is an inner block: descent to the next block
				libchain::apply([c] () -> bool {
					if(c->parentNumber != -1)
						c->self->p_pageCache.releasePage(c->parentNumber);
					c->parentBuffer = c->currentBuffer;
					c->parentNumber = c->currentNumber;

					return true;
				})
			);

			return libchain::repeat(insert_or_descend);
		}, Context(this, key, value, compare));
	}

	class FindClosure {
	public:
		FindClosure(Btree *tree) : p_tree(tree), p_searchClosure(tree) { }

		void findFirst(Async::Callback<void(Ref)> on_complete);
		void findNext(UnaryCompareCallback compare,
				Async::Callback<void(Ref)> on_complete);
		void findPrev(UnaryCompareCallback compare,
				Async::Callback<void(Ref)> on_complete);

	private:
		void findFirstDescend();
		void findFirstOnRead(char *buffer);
		void findNextDescend();
		void findNextOnRead(char *buffer);
		void findNextOnFoundChild(int index);
		void findNextInLeaf(int index);
		void findPrevDescend();
		void findPrevOnRead(char *buffer);
		void findPrevOnFoundChild(int index);
		void findPrevInLeaf(int index);
		void findPrevReadLeft(char *buffer);

		Btree *p_tree;
		UnaryCompareCallback p_compare;
		Async::Callback<void(Ref)> p_onComplete;

		BlkIndexType p_blockNumber;
		char *p_blockBuffer;

		SearchNodeClosure p_searchClosure;
	};
	
	void setPath(const std::string &path) {
		p_path = path;
	}
	std::string getPath() {
		return p_path;
	}
	
	void setCompare(BinaryCompareCallback compare) {
		p_compare = compare;
	}
	void setWriteKey(WriteKeyCallback write_key) {
		p_writeKey = write_key;
	}
	void setReadKey(ReadKeyCallback read_key) {
		p_readKey = read_key;
	}

	void createTree() {
		p_pageCache.open(p_path + "/" + p_name + ".btree");

		p_curFileHead.rootBlock = 1;
		p_curFileHead.numBlocks = 2;
		p_curFileHead.depth = 1;

		p_pageCache.initializePage(1, ASYNC_MEMBER(this, &Btree::createOnInitialize));
	}
	void createOnInitialize(char *root_block) {
		p_headSetFlags(root_block, BlockHead::kFlagIsLeaf);
		p_leafSetEntCount(root_block, 0);
		p_leafSetLeftLink(root_block, 0);
		p_leafSetRightLink(root_block, 0);
		p_pageCache.writePage(1);
		p_pageCache.releasePage(1);
	}

//FIXME: this function should either be asynchronous
// or there should be a synchronous read block function
/*	void loadTree() {
		p_pageCache.open(p_path + "/" + p_name + ".btree");
		
		readBlockSync(0, ASYNC_MEMBER(this, &Btree::onLoadFileHead));
	}
	void onLoadFileHead(char *desc_block) {
		FileHead *file_head = (FileHead*)desc_block;
		p_curFileHead.rootBlock = OS::fromLeU32(file_head->rootBlock);
		p_curFileHead.numBlocks = OS::fromLeU32(file_head->numBlocks);
		p_curFileHead.depth = OS::fromLeU32(file_head->depth);
		p_pageCache.releasePage(0);
	}*/
	
	void closeTree() {

	}
	
	void integrity(KeyType min, KeyType max) {
		p_blockIntegrity(p_curFileHead.rootBlock, min, max);
	}
	
	int32_t getDepth() {
		return p_curFileHead.depth;
	}

private:
	typedef uint32_t flags_type;
	struct FileHead {
		BlkIndexType rootBlock;
		BlkIndexType numBlocks;
		BlkIndexType depth;
	};
	struct BlockHead {
		enum {
			kFlagIsLeaf = 1
		};

		flags_type flags;
	};
	struct InnerHead {
		BlockHead blockHead;
		BlkIndexType entCount2;
	};
	struct LeafHead {
		BlockHead blockHead;
		BlkIndexType entCount2;
		BlkIndexType leftLink2;
		BlkIndexType rightLink2;
	};

	BinaryCompareCallback p_compare;
	ReadKeyCallback p_readKey;
	WriteKeyCallback p_writeKey;

	std::string p_path;
	std::string p_name;
	PageCache p_pageCache;
	
	size_t p_blockSize;
	size_t p_keySize;
	size_t p_valSize;

	FileHead p_curFileHead;

	size_t p_entsPerInner() {
		return (p_blockSize - sizeof(InnerHead) - sizeof(BlkIndexType))
			/ (p_keySize + sizeof(BlkIndexType));
	}
	size_t p_entsPerLeaf() {
		return (p_blockSize - sizeof(LeafHead))
			/ (p_keySize + p_valSize);
	}

	// size of an entry in an inner node
	// entries consist of key + reference to child block
	size_t p_entSizeInner() {
		return p_keySize + sizeof(BlkIndexType);
	}
	// offset of an entry in an inner node
	size_t p_entOffInner(int i) {
		return sizeof(InnerHead) + sizeof(BlkIndexType)
				+ p_entSizeInner() * i;
	}
	// offset of the leftmost child reference
	// it does not belong to an entry and thus must be special cased
	size_t p_lrefOffInner() {
		return sizeof(InnerHead);
	}
	size_t p_keyOffInner(int i) {
		return p_entOffInner(i);
	}
	size_t p_refOffInner(int i) {
		return p_entOffInner(i) + p_keySize;
	}

	size_t p_entSizeLeaf() {
		return p_keySize + p_valSize;
	}
	size_t p_entOffLeaf(int i) {
		return sizeof(LeafHead) + p_entSizeLeaf() * i;
	}
	size_t p_valOffLeaf(int i) {
		return p_entOffLeaf(i);
	}
	size_t p_keyOffLeaf(int i) {
		return p_entOffLeaf(i) + p_valSize;
	}
	
	flags_type p_headGetFlags(char *block_buf);
	void p_headSetFlags(char *block_buf, flags_type flags);

	BlkIndexType p_innerGetEntCount(char *block_buf);
	void p_innerSetEntCount(char *block_buf, BlkIndexType ent_count);
	
	BlkIndexType p_leafGetEntCount(char *block_buf);
	void p_leafSetEntCount(char *block_buf, BlkIndexType ent_count);
	BlkIndexType p_leafGetLeftLink(char *block_buf);
	void p_leafSetLeftLink(char *block_buf, BlkIndexType link);
	BlkIndexType p_leafGetRightLink(char *block_buf);
	void p_leafSetRightLink(char *block_buf, BlkIndexType link);

	void p_insertAtLeaf(char *block, BlkIndexType i,
			const KeyType &key, void *value);
	void p_insertAtInnerR(char *block, int i,
			const KeyType &key, BlkIndexType ref);
	
	BlkIndexType p_allocBlock() {	
		BlkIndexType number = p_curFileHead.numBlocks;
		p_curFileHead.numBlocks++;
		return number;
	}

	bool blockIsFull(char *block_buf);

	void p_blockIntegrity(BlkIndexType block_num,
			KeyType min, KeyType max);
	
	//FIXME: write file head on close
	void p_writeFileHead() {
		p_pageCache.initializePage(0, ASYNC_MEMBER(this, &Btree::writeHeadOnInitialize));
	}
	void writeHeadOnInitialize(char *desc_block) {
		FileHead *file_head = (FileHead*)desc_block;
		file_head->rootBlock = OS::toLeU32(p_curFileHead.rootBlock);
		file_head->numBlocks = OS::toLeU32(p_curFileHead.numBlocks);
		file_head->depth = OS::toLeU32(p_curFileHead.depth);
		p_pageCache.writePage(0);
		p_pageCache.releasePage(0);
	}
};

template<typename KeyType>
Btree<KeyType>::Btree(std::string name, size_t block_size,
		size_t key_size, size_t val_size,
		CacheHost *cache_host, TaskPool *io_pool)
	: p_name(name), p_pageCache(cache_host, block_size, io_pool), p_blockSize(block_size),
		p_keySize(key_size), p_valSize(val_size) {
	assert(p_blockSize > sizeof(FileHead)
			&& p_blockSize > sizeof(InnerHead)
			&& p_blockSize > sizeof(LeafHead));
	assert(p_entsPerInner() >= 4);
	assert(p_entsPerLeaf() >= 4);

//	std::cout << "ents per inner: " << p_entsPerInner()
//			<< ", leaf: " << p_entsPerLeaf() << std::endl;
}

/* ------------------------------------------------------------------------- *
 * REF AND SEQUENCE FUNCTIONS                                                 *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
Btree<KeyType>::IterateClosure::~IterateClosure() {
	if(p_block > 0)
		p_tree->p_pageCache.releasePage(p_block);
}

template<typename KeyType>
typename Btree<KeyType>::Ref Btree<KeyType>::IterateClosure::position() {
	return Ref(p_block, p_entry);
}

template<typename KeyType>
void Btree<KeyType>::IterateClosure::seek(Ref ref, Async::Callback<void()> callback) {
	p_callback = callback;
	
	if(p_block > 0)
		p_tree->p_pageCache.releasePage(p_block);
	p_block = ref.block;
	p_entry = ref.entry;

	if(ref.block > 0) {
		p_tree->p_pageCache.readPage(ref.block,
				ASYNC_MEMBER(this, &IterateClosure::seekOnRead));
	}else{
		p_callback();
	}
}
template<typename KeyType>
void Btree<KeyType>::IterateClosure::seekOnRead(char *buffer) {
	p_buffer = buffer;
	p_callback();
}
template<typename KeyType>
void Btree<KeyType>::IterateClosure::forward(Async::Callback<void()> callback) {
	p_callback = callback;

	assert(p_block > 0 && p_entry < p_tree->p_leafGetEntCount(p_buffer));
	p_entry++;

	BlkIndexType right_link = p_tree->p_leafGetRightLink(p_buffer);
	if(p_entry == p_tree->p_leafGetEntCount(p_buffer)) {
		if(right_link != 0) {
			p_tree->p_pageCache.releasePage(p_block);
			p_block = right_link;
			p_entry = 0;
			p_tree->p_pageCache.readPage(right_link,
					ASYNC_MEMBER(this, &IterateClosure::forwardOnRead));
		}else{
			p_tree->p_pageCache.releasePage(p_block);
			p_block = -1;
			p_entry = -1;
			p_callback();
		}
	}else{
		p_callback();
	}
}
template<typename KeyType>
void Btree<KeyType>::IterateClosure::forwardOnRead(char *buffer) {
	p_buffer = buffer;
	p_callback();
}

template<typename KeyType>
KeyType Btree<KeyType>::IterateClosure::getKey() {
	assert(p_block > 0 && p_entry >= 0);
	return p_tree->p_readKey(p_buffer + p_tree->p_keyOffLeaf(p_entry));
}

template<typename KeyType>
void Btree<KeyType>::IterateClosure::getValue(void *value) {
	assert(p_block > 0 && p_entry >= 0);
	std::memcpy(value, p_buffer + p_tree->p_valOffLeaf(p_entry),
			p_tree->p_valSize);
}
template<typename KeyType>
void Btree<KeyType>::IterateClosure::setValue(void *value) {
	assert(p_block > 0 && p_entry >= 0);
	std::memcpy(p_buffer + p_tree->p_valOffLeaf(p_entry), value,
			p_tree->p_valSize);
	p_tree->p_pageCache.writePage(p_block);
}

/* ------------------------------------------------------------------------- *
 * INSERT AND FIND FUNCTIONS                                                 *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::FindClosure::findFirst(Async::Callback<void(Ref)> on_complete) {
	p_onComplete = on_complete;

	p_blockNumber = p_tree->p_curFileHead.rootBlock;

	findFirstDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findFirstDescend() {
	p_tree->p_pageCache.readPage(p_blockNumber,
			ASYNC_MEMBER(this, &FindClosure::findFirstOnRead));
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findFirstOnRead(char *buffer) {
	p_blockBuffer = buffer;

	flags_type flags = p_tree->p_headGetFlags(p_blockBuffer);
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		if(p_tree->p_leafGetEntCount(p_blockBuffer) != 0) {
			p_tree->p_pageCache.releasePage(p_blockNumber);

			Ref result;
			result.block = p_blockNumber;
			result.entry = 0;
			
			p_onComplete(result);
			return;
		}else{
			p_tree->p_pageCache.releasePage(p_blockNumber);

			p_onComplete(Ref());
			return;
		}
	}
	
	char *ref_ptr = p_blockBuffer + p_tree->p_lrefOffInner();
	BlkIndexType child_number = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
	p_tree->p_pageCache.releasePage(p_blockNumber);
	p_blockNumber = child_number;

	findFirstDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findNext(UnaryCompareCallback compare,
		Async::Callback<void(Ref)> on_complete) {
	p_compare = compare;
	p_onComplete = on_complete;

	p_blockNumber = p_tree->p_curFileHead.rootBlock;

	findNextDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findNextDescend() {
	p_tree->p_pageCache.readPage(p_blockNumber,
			ASYNC_MEMBER(this, &FindClosure::findNextOnRead));
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findNextOnRead(char *buffer) {
	p_blockBuffer = buffer;

	flags_type flags = p_tree->p_headGetFlags(p_blockBuffer);
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		p_searchClosure.nextInLeaf(p_blockBuffer, p_compare,
				ASYNC_MEMBER(this, &FindClosure::findNextInLeaf));
		return;
	}

	p_searchClosure.nextInInner(p_blockBuffer, p_compare,
			ASYNC_MEMBER(this, &FindClosure::findNextOnFoundChild));
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findNextOnFoundChild(int index) {
	if(index == -1) {
		BlkIndexType ent_count = p_tree->p_innerGetEntCount(p_blockBuffer);
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(ent_count - 1);
		BlkIndexType child_num = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_blockNumber = child_num;
	}else if(index == 0) {
		char *ref_ptr = p_blockBuffer + p_tree->p_lrefOffInner();
		BlkIndexType child_num = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_blockNumber = child_num;
	}else{
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(index - 1);
		BlkIndexType child_num = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
		p_blockNumber = child_num;
	}

	findNextDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findNextInLeaf(int index) {
	p_tree->p_pageCache.releasePage(p_blockNumber);

	if(index >= 0) {
		p_onComplete(Ref(p_blockNumber, index));
	}else{
		p_onComplete(Ref());
	}
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrev(UnaryCompareCallback compare,
		Async::Callback<void(Ref)> on_complete) {
	p_compare = compare;
	p_onComplete = on_complete;

	p_blockNumber = p_tree->p_curFileHead.rootBlock;

	findPrevDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevDescend() {
	p_tree->p_pageCache.readPage(p_blockNumber,
			ASYNC_MEMBER(this, &FindClosure::findPrevOnRead));
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevOnRead(char *buffer) {
	p_blockBuffer = buffer;

	flags_type flags = p_tree->p_headGetFlags(p_blockBuffer);
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		p_searchClosure.prevInLeaf(p_blockBuffer, p_compare,
				ASYNC_MEMBER(this, &FindClosure::findPrevInLeaf));
		return;
	}

	p_searchClosure.prevInInner(p_blockBuffer, p_compare,
			ASYNC_MEMBER(this, &FindClosure::findPrevOnFoundChild));
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevOnFoundChild(int index) {
	if(index == -1) {
		char *ref_ptr = p_blockBuffer + p_tree->p_lrefOffInner();
		BlkIndexType child_num = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_blockNumber = child_num;
	}else{
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(index);
		BlkIndexType child_num = OS::fromLeU32(*((BlkIndexType*)ref_ptr));
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_blockNumber = child_num;
	}

	findPrevDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevInLeaf(int index) {
	if(index == -1) {
		BlkIndexType left_link = p_tree->p_leafGetLeftLink(p_blockBuffer);
		if(left_link != 0) {
			p_tree->p_pageCache.releasePage(p_blockNumber);
			p_blockNumber = left_link;
			p_tree->p_pageCache.readPage(p_blockNumber,
					ASYNC_MEMBER(this, &FindClosure::findPrevReadLeft));
		}else{
			p_tree->p_pageCache.releasePage(p_blockNumber);
			p_onComplete(Ref());
		}
	}else{
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_onComplete(Ref(p_blockNumber, index));
	}
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevReadLeft(char *buffer) {
	p_blockBuffer = buffer;

	BlkIndexType ent_count = p_tree->p_leafGetEntCount(p_blockBuffer);
	assert(ent_count > 0);

	p_tree->p_pageCache.releasePage(p_blockNumber);
	p_onComplete(Ref(p_blockNumber, ent_count - 1));
}

/* ------------------------------------------------------------------------- *
 * NODE SPLITTING FUNCTIONS                                                  *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
bool Btree<KeyType>::blockIsFull(char *block_buf) {
	flags_type flags = OS::fromLeU32(*((flags_type*)block_buf));
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		if(p_leafGetEntCount(block_buf) >= p_entsPerLeaf() - 1) {
			return true;
		}
	}else{
		if(p_innerGetEntCount(block_buf) >= p_entsPerInner() - 1) {
			return true;
		}
	}
	return false;
}

template<typename KeyType>
void Btree<KeyType>::SplitClosure::split(BlkIndexType block_num, char *block_buf,
		char *parent_buf, BlkIndexType block_index,
		Async::Callback<void()> on_complete) {
	p_blockNumber = block_num;
	p_blockBuffer = block_buf;
	p_parentBuffer = parent_buf;
	p_blockIndex = block_index;
	p_onComplete = on_complete;

	flags_type flags = OS::fromLeU32(*((flags_type*)block_buf));
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		if(p_tree->p_leafGetEntCount(block_buf) >= p_tree->p_entsPerLeaf() - 1) {
			splitLeaf();
			return;
		}
	}else{
		if(p_tree->p_innerGetEntCount(block_buf) >= p_tree->p_entsPerInner() - 1) {
			splitInner();
			return;
		}
	}
	throw std::logic_error("No need to split");
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::splitLeaf() {
//	std::cout << "split leaf" << std::endl;
	p_splitNumber = p_tree->p_allocBlock();

	BlkIndexType ent_count = p_tree->p_leafGetEntCount(p_blockBuffer);
	p_rightLinkNumber = p_tree->p_leafGetRightLink(p_blockBuffer);
	p_leftSize = ent_count / 2;
	p_rightSize = ent_count - p_leftSize;
	assert(p_leftSize > 0 && p_rightSize > 0
			&& p_leftSize < p_tree->p_entsPerLeaf() - 1
			&& p_rightSize < p_tree->p_entsPerLeaf() - 1);
	p_tree->p_leafSetEntCount(p_blockBuffer, p_leftSize);
	p_tree->p_leafSetRightLink(p_blockBuffer, p_splitNumber);
//		std::cout << "split leaf into: " << p_leftSize << ", " << p_rightSize << std::endl;
	
	p_tree->p_pageCache.initializePage(p_splitNumber,
			ASYNC_MEMBER(this, &SplitClosure::splitLeafOnInitialize));
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::splitLeafOnInitialize(char *split_block) {
	p_tree->p_headSetFlags(split_block, BlockHead::kFlagIsLeaf);
	p_tree->p_leafSetEntCount(split_block, p_rightSize);
	p_tree->p_leafSetLeftLink(split_block, p_blockNumber);
	p_tree->p_leafSetRightLink(split_block, p_rightLinkNumber);

	/* setup the entries of the new block */
	p_splitKey = p_tree->p_readKey(p_blockBuffer + p_tree->p_keyOffLeaf(p_leftSize - 1));
	std::memcpy(split_block + p_tree->p_entOffLeaf(0),
			p_blockBuffer + p_tree->p_entOffLeaf(p_leftSize),
			p_rightSize * p_tree->p_entSizeLeaf());
	/* FIXME: for debugging only! */
	std::memset(p_blockBuffer + p_tree->p_entOffLeaf(p_leftSize), 0, p_rightSize * p_tree->p_entSizeLeaf());
	p_tree->p_pageCache.writePage(p_splitNumber);
	p_tree->p_pageCache.releasePage(p_splitNumber);

	if(p_rightLinkNumber != 0) {
		p_tree->p_pageCache.readPage(p_rightLinkNumber,
				ASYNC_MEMBER(this, &SplitClosure::onReadRightLink));
	}else{
		fixParent();
	}
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::onReadRightLink(char *link_buffer) {
	p_tree->p_leafSetLeftLink(link_buffer, p_splitNumber);
	p_tree->p_pageCache.writePage(p_rightLinkNumber);
	p_tree->p_pageCache.releasePage(p_rightLinkNumber);

	fixParent();
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::splitInner() {
//	std::cout << "split inner" << std::endl;
	p_splitNumber = p_tree->p_allocBlock();

	/* note: p_leftSize + p_rightSize = ent_count - 1
		because one of the keys moves to the parent node */
	BlkIndexType ent_count = p_tree->p_innerGetEntCount(p_blockBuffer);
	p_leftSize = ent_count / 2;
	p_rightSize = ent_count - p_leftSize - 1;
	assert(p_leftSize > 0 && p_rightSize > 0
			&& p_leftSize < p_tree->p_entsPerInner() - 2
			&& p_rightSize < p_tree->p_entsPerInner() - 1);
	p_tree->p_innerSetEntCount(p_blockBuffer, p_leftSize);
//		std::cout << "split inner into: " << p_leftSize << ", " << p_rightSize << std::endl;
	
	p_tree->p_pageCache.initializePage(p_splitNumber,
			ASYNC_MEMBER(this, &SplitClosure::splitInnerOnInitialize));
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::splitInnerOnInitialize(char *split_block) {
	p_tree->p_headSetFlags(split_block, 0);
	p_tree->p_innerSetEntCount(split_block, p_rightSize);	
	
	/* setup the leftmost child reference of the new block */
	char *block_rref = p_blockBuffer + p_tree->p_refOffInner(p_leftSize);
	char *split_lref = split_block + p_tree->p_lrefOffInner();
	*(BlkIndexType*)split_lref = *(BlkIndexType*)block_rref;
	p_splitKey = p_tree->p_readKey(p_blockBuffer + p_tree->p_keyOffInner(p_leftSize));

	/* setup the remaining entries of the new block */
	std::memcpy(split_block + p_tree->p_entOffInner(0),
			p_blockBuffer + p_tree->p_entOffInner(p_leftSize + 1),
			p_rightSize * p_tree->p_entSizeInner());
	/* FIXME: for debugging only! */
	std::memset(p_blockBuffer + p_tree->p_entOffInner(p_leftSize), 0, (p_rightSize + 1) * p_tree->p_entSizeInner());
	p_tree->p_pageCache.writePage(p_splitNumber);
	p_tree->p_pageCache.releasePage(p_splitNumber);
	
	fixParent();
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::fixParent() {
	if(p_parentBuffer != nullptr) {
		BlkIndexType parent_count = p_tree->p_innerGetEntCount(p_parentBuffer);
		assert(parent_count < p_tree->p_entsPerInner());

		p_tree->p_insertAtInnerR(p_parentBuffer, p_blockIndex + 1, p_splitKey, p_splitNumber);
		
		p_onComplete();
	}else{
		p_newRootNumber = p_tree->p_allocBlock();
		p_tree->p_curFileHead.rootBlock = p_newRootNumber;
		p_tree->p_curFileHead.depth++;
		
		p_tree->p_pageCache.initializePage(p_newRootNumber,
				ASYNC_MEMBER(this, &SplitClosure::fixParentOnInitialize));
	}
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::fixParentOnInitialize(char *new_root_buffer) {
	p_tree->p_headSetFlags(new_root_buffer, 0);
	p_tree->p_innerSetEntCount(new_root_buffer, 1);
	
	char *lref = new_root_buffer + p_tree->p_lrefOffInner();
	char *ref0 = new_root_buffer + p_tree->p_refOffInner(0);
	*((BlkIndexType*)lref) = OS::toLeU32(p_blockNumber);
	*((BlkIndexType*)ref0) = OS::toLeU32(p_splitNumber);
	p_tree->p_writeKey(new_root_buffer + p_tree->p_keyOffInner(0), p_splitKey);

	p_tree->p_pageCache.writePage(p_newRootNumber);
	p_tree->p_pageCache.releasePage(p_newRootNumber);

	p_onComplete();
}

/* ------------------------------------------------------------------------- *
 * INTERNAL UTILITY FUNCTIONS                                                *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInLeaf(char *block,
		UnaryCompareCallback compare,
		Async::Callback<void(int)> callback) {
	p_blockBuffer = block;
	p_compare = compare;
	p_callback = callback;

	p_entryCount = p_tree->p_leafGetEntCount(p_blockBuffer);
	p_entryIndex = p_entryCount;

	prevInLeafLoop();
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInLeafLoop() {
	if(p_entryIndex > 0) {
		KeyType ent_key = p_tree->p_readKey(p_blockBuffer
				+ p_tree->p_keyOffLeaf(p_entryIndex - 1));
		p_compare(ent_key, ASYNC_MEMBER(this, &SearchNodeClosure::prevInLeafCheck));
	}else{
		p_callback(-1);
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInLeafCheck(int result) {
	if(result <= 0) {
		p_callback(p_entryIndex - 1);
	}else{
		p_entryIndex--;
		//TODO: use nextTick() to prevent stack overflow
		prevInLeafLoop();
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInInner(char *block,
		UnaryCompareCallback compare,
		Async::Callback<void(int)> callback) {
	p_blockBuffer = block;
	p_compare = compare;
	p_callback = callback;

	p_entryCount = p_tree->p_innerGetEntCount(p_blockBuffer);
	p_entryIndex = p_entryCount;
	
	prevInInnerLoop();
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInInnerLoop() {
	if(p_entryIndex > 0) {
		KeyType ent_key = p_tree->p_readKey(p_blockBuffer
				+ p_tree->p_keyOffInner(p_entryIndex - 1));
		p_compare(ent_key, ASYNC_MEMBER(this, &SearchNodeClosure::prevInInnerCheck));
	}else{
		p_callback(-1);
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::prevInInnerCheck(int result) {
	if(result <= 0) {
		p_callback(p_entryIndex - 1);
	}else{
		p_entryIndex--;
		//TODO: use nextTick() to prevent stack overflow
		prevInInnerLoop();
	}
}

template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInLeaf(char *block,
		UnaryCompareCallback compare,
		Async::Callback<void(int)> callback) {
	p_blockBuffer = block;
	p_compare = compare;
	p_callback = callback;

	p_entryCount = p_tree->p_leafGetEntCount(p_blockBuffer);
	p_entryIndex = 0;

	nextInLeafLoop();
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInLeafLoop() {
	if(p_entryIndex < p_entryCount) {
		KeyType ent_key = p_tree->p_readKey(p_blockBuffer
				+ p_tree->p_keyOffLeaf(p_entryIndex));
		p_compare(ent_key, ASYNC_MEMBER(this, &SearchNodeClosure::nextInLeafCheck));
	}else{
		p_callback(-1);
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInLeafCheck(int result) {
	if(result >= 0) {
		p_callback(p_entryIndex);
	}else{
		p_entryIndex++;
		//TODO: use nextTick() to prevent stack overflow
		nextInLeafLoop();
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInInner(char *block,
		UnaryCompareCallback compare,
		Async::Callback<void(int)> callback) {
	p_blockBuffer = block;
	p_compare = compare;
	p_callback = callback;
	
	p_entryCount = p_tree->p_innerGetEntCount(p_blockBuffer);
	p_entryIndex = 0;

	nextInInnerLoop();
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInInnerLoop() {
	if(p_entryIndex < p_entryCount) {
		KeyType ent_key = p_tree->p_readKey(p_blockBuffer
				+ p_tree->p_keyOffInner(p_entryIndex));
		p_compare(ent_key, ASYNC_MEMBER(this, &SearchNodeClosure::nextInInnerCheck));
	}else{
		p_callback(-1);
	}
}
template<typename KeyType>
void Btree<KeyType>::SearchNodeClosure::nextInInnerCheck(int result) {
	if(result >= 0) {
		p_callback(p_entryIndex);
		return;
	}else{
		p_entryIndex++;
		//TODO: use nextTick() to prevent stack overflow
		nextInInnerLoop();
	}
}

template<typename KeyType>
void Btree<KeyType>::p_insertAtLeaf(char *block, BlkIndexType i,
		const KeyType &key, void *value) {
	BlkIndexType ent_count = p_leafGetEntCount(block);
	std::memmove(block + p_entOffLeaf(i + 1), block + p_entOffLeaf(i),
		(ent_count - i) * p_entSizeLeaf());
	p_writeKey(block + p_keyOffLeaf(i), key);
	std::memcpy(block + p_valOffLeaf(i), value, p_valSize);
	p_leafSetEntCount(block, ent_count + 1);
}
template<typename KeyType>
void Btree<KeyType>::p_insertAtInnerR(char *block, int i,
		const KeyType &key, BlkIndexType ref) {
	BlkIndexType ent_count = p_innerGetEntCount(block);
	std::memmove(block + p_entOffInner(i + 1), block + p_entOffInner(i),
		(ent_count - i) * p_entSizeInner());
	p_writeKey(block + p_keyOffInner(i), key);
	char *refi = block + p_refOffInner(i);
	*((BlkIndexType*)refi) = ref;
	p_innerSetEntCount(block, ent_count + 1);
}

/* ------------------------------------------------------------------------- *
 * DEBUGGING FUNCTIONS                                                       *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::p_blockIntegrity(BlkIndexType block_num,
		KeyType min, KeyType max) {
	char *block_buf = new char[p_blockSize];
	//FIXME: use async read instead
	//p_readBlock(block_num, block_buf);
	
	flags_type flags = OS::fromLeU32(*(flags_type*)block_buf);
	if((flags & BlockHead::kFlagIsLeaf) != 0) {
		BlkIndexType ent_count = p_innerGetEntCount(block_buf);
		KeyType prev_key = min;
		for(int i = 0; i < ent_count; i++) {
			KeyType key = p_readKey(block_buf + p_keyOffLeaf(i));
			if(p_compare(key, prev_key) < 0)
				throw std::logic_error(std::string("Integrity violated!"
						" (1.1), block: ") + std::to_string(block_num));
			prev_key = key;
		}
		if(p_compare(max, prev_key) < 0)
			throw std::logic_error(std::string("Integrity violated!"
					" (1.2), block: ") + std::to_string(block_num));
	}else{
		BlkIndexType ent_count = p_innerGetEntCount(block_buf);
		KeyType prev_key = min;
		for(int i = 0; i < ent_count; i++) {
			KeyType key = p_readKey(block_buf + p_keyOffInner(i));
			if(p_compare(key, prev_key) < 0)
				throw std::logic_error(std::string("Integrity violated!"
						" (2.1), block: ") + std::to_string(block_num));
			prev_key = key;
		}
		if(p_compare(max, prev_key) < 0)
			throw std::logic_error(std::string("Integrity violated!"
					" (2.2), block: ") + std::to_string(block_num));
		
		char *lref = block_buf + p_lrefOffInner();
		p_blockIntegrity(*(BlkIndexType*)lref, min,
				p_readKey(block_buf + p_keyOffInner(0)));
		for(int i = 0; i < ent_count; i++) {
			KeyType ckey = p_readKey(block_buf + p_keyOffInner(i));
			KeyType nkey = (i == ent_count - 1 ? max
				: p_readKey(block_buf + p_keyOffInner(i + 1)));
			char *refi = block_buf + p_refOffInner(i);
			p_blockIntegrity(*(BlkIndexType*)refi, ckey, nkey);
		}
	}
}


/* ------------------------------------------------------------------------- *
 * INTERNAL TECHNICAL UTILITY FUNCTIONS                                      *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
typename Btree<KeyType>::flags_type Btree<KeyType>::p_headGetFlags(char *block_buf) {
	BlockHead *bhead = (BlockHead*)block_buf;
	return OS::fromLeU32(bhead->flags);
}
template<typename KeyType>
void Btree<KeyType>::p_headSetFlags(char *block_buf,
		typename Btree<KeyType>::flags_type flags) {
	BlockHead *bhead = (BlockHead*)block_buf;
	bhead->flags = OS::toLeU32(flags);
}

template<typename KeyType>
typename Btree<KeyType>::BlkIndexType Btree<KeyType>::p_innerGetEntCount(char *block_buf) {
	InnerHead *head = (InnerHead*)block_buf;
	return OS::fromLeU32(head->entCount2);
}
template<typename KeyType>
void Btree<KeyType>::p_innerSetEntCount(char *block_buf,
		typename Btree<KeyType>::BlkIndexType ent_count) {
	InnerHead *head = (InnerHead*)block_buf;
	head->entCount2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::BlkIndexType Btree<KeyType>::p_leafGetEntCount(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->entCount2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetEntCount(char *block_buf,
		typename Btree<KeyType>::BlkIndexType ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->entCount2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::BlkIndexType Btree<KeyType>::p_leafGetLeftLink(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->leftLink2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetLeftLink(char *block_buf,
		typename Btree<KeyType>::BlkIndexType ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->leftLink2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::BlkIndexType Btree<KeyType>::p_leafGetRightLink(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->rightLink2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetRightLink(char *block_buf,
		typename Btree<KeyType>::BlkIndexType ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->rightLink2 = OS::toLeU32(ent_count);
}

#endif

