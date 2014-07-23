
#ifndef D3B_LL_BTREE_HPP
#define D3B_LL_BTREE_HPP

#include <cstring>

#include "Ll/PageCache.hpp"

template<typename KeyType>
class Btree {
public:
	typedef int32_t blknum_type;

	typedef Async::Callback<void(void *, const KeyType &)> WriteKeyCallback;
	typedef Async::Callback<KeyType(const void*)> ReadKeyCallback;
	typedef Async::Callback<void(const KeyType &, Async::Callback<void(int)>)> UnaryCompareCallback;
	typedef Async::Callback<int(const KeyType &, const KeyType &)> BinaryCompareCallback;

	Btree(std::string name,
			Linux::size_type block_size,
			Linux::size_type key_size,
			Linux::size_type val_size);

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
		Ref(blknum_type block, blknum_type entry)
				: block(block), entry(entry) {
		}
		
		blknum_type block;
		blknum_type entry;
	};

	// FIXME: IterateClosure should properly deallocate memory
	class IterateClosure {
	public:
		IterateClosure(Btree *tree) : p_tree(tree), p_block(0), p_entry(0),
				p_buffer(nullptr) { }
		~IterateClosure() {
		}
		
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

		blknum_type p_block;
		blknum_type p_entry;
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
		
		blknum_type p_entryCount;
		blknum_type p_entryIndex;
		char *p_blockBuffer;
	};

	class SplitClosure {
	public:
		SplitClosure(Btree *tree) : p_tree(tree) { }

		void split(blknum_type block_num, char *block_buf,
				char *parent_buf, blknum_type block_index,
				Async::Callback<void()> on_complete);
	
	private:
		void splitLeaf();
		void onReadRightLink(char *link_buffer);
		void splitInner();
		void fixParent();

		Btree *p_tree;
		Async::Callback<void()> p_onComplete;
		
		blknum_type p_blockNumber;
		blknum_type p_blockIndex;
		blknum_type p_splitNumber;
		blknum_type p_rightLinkNumber;
		char *p_blockBuffer;
		char *p_parentBuffer;
		KeyType p_splitKey;
	};

	class InsertClosure {
	public:
		InsertClosure(Btree *tree) : p_tree(tree),
				p_splitClosure(tree), p_searchClosure(tree) { }

		void insert(KeyType *key, void *value,
				UnaryCompareCallback compare,
				Async::Callback<void()> on_complete);

	private:
		void onReadRoot(char *buffer);
		void onSplitRoot();
		void descend();
		void onFoundChild(int index);
		void onReadChild(char *buffer);
		void onSplitChild();
		void insertIntoNode(int index);

		Btree *p_tree;
		KeyType *p_keyPtr;
		void *p_valuePtr;
		UnaryCompareCallback p_compare;
		Async::Callback<void()> p_onComplete;

		blknum_type p_blockNumber;
		blknum_type p_childNumber;
		blknum_type p_childIndex;
		char *p_blockBuffer;
		char *p_childBuffer;
		
		SplitClosure p_splitClosure;
		SearchNodeClosure p_searchClosure;
	};

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

		blknum_type p_blockNumber;
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
		p_writeFileHead();

		char *root_block = p_pageCache.initializePage(1);
		p_headSetFlags(root_block, 1);
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
		blknum_type rootBlock;
		blknum_type numBlocks;
		blknum_type depth;
	};
	struct BlockHead {
		flags_type flags;
	};
	struct InnerHead {
		BlockHead blockHead;
		blknum_type entCount2;
	};
	struct LeafHead {
		BlockHead blockHead;
		blknum_type entCount2;
		blknum_type leftLink2;
		blknum_type rightLink2;
	};

	BinaryCompareCallback p_compare;
	ReadKeyCallback p_readKey;
	WriteKeyCallback p_writeKey;

	std::string p_path;
	std::string p_name;
	PageCache p_pageCache;
	
	Linux::size_type p_blockSize;
	Linux::size_type p_keySize;
	Linux::size_type p_valSize;

	FileHead p_curFileHead;

	Linux::size_type p_entsPerInner() {
		return (p_blockSize - sizeof(InnerHead) - sizeof(blknum_type))
			/ (p_keySize + sizeof(blknum_type));
	}
	Linux::size_type p_entsPerLeaf() {
		return (p_blockSize - sizeof(LeafHead))
			/ (p_keySize + p_valSize);
	}

	// size of an entry in an inner node
	// entries consist of key + reference to child block
	Linux::size_type p_entSizeInner() {
		return p_keySize + sizeof(blknum_type);
	}
	// offset of an entry in an inner node
	Linux::size_type p_entOffInner(int i) {
		return sizeof(InnerHead) + sizeof(blknum_type)
				+ p_entSizeInner() * i;
	}
	// offset of the leftmost child reference
	// it does not belong to an entry and thus must be special cased
	Linux::size_type p_lrefOffInner() {
		return sizeof(InnerHead);
	}
	Linux::size_type p_keyOffInner(int i) {
		return p_entOffInner(i);
	}
	Linux::size_type p_refOffInner(int i) {
		return p_entOffInner(i) + p_keySize;
	}

	Linux::size_type p_entSizeLeaf() {
		return p_keySize + p_valSize;
	}
	Linux::size_type p_entOffLeaf(int i) {
		return sizeof(LeafHead) + p_entSizeLeaf() * i;
	}
	Linux::size_type p_valOffLeaf(int i) {
		return p_entOffLeaf(i);
	}
	Linux::size_type p_keyOffLeaf(int i) {
		return p_entOffLeaf(i) + p_valSize;
	}
	
	flags_type p_headGetFlags(char *block_buf);
	void p_headSetFlags(char *block_buf, flags_type flags);

	blknum_type p_innerGetEntCount(char *block_buf);
	void p_innerSetEntCount(char *block_buf, blknum_type ent_count);
	
	blknum_type p_leafGetEntCount(char *block_buf);
	void p_leafSetEntCount(char *block_buf, blknum_type ent_count);
	blknum_type p_leafGetLeftLink(char *block_buf);
	void p_leafSetLeftLink(char *block_buf, blknum_type link);
	blknum_type p_leafGetRightLink(char *block_buf);
	void p_leafSetRightLink(char *block_buf, blknum_type link);

	void p_insertAtLeaf(char *block, blknum_type i,
			const KeyType &key, void *value);
	void p_insertAtInnerR(char *block, int i,
			const KeyType &key, blknum_type ref);
	
	blknum_type p_allocBlock() {	
		blknum_type number = p_curFileHead.numBlocks;
		p_curFileHead.numBlocks++;
		p_writeFileHead();
		return number;
	}

	bool blockIsFull(blknum_type block_num, char *block_buf);

	void p_blockIntegrity(blknum_type block_num,
			KeyType min, KeyType max);
	
	void p_writeFileHead() {
		char *desc_block = p_pageCache.initializePage(0);
		FileHead *file_head = (FileHead*)desc_block;
		file_head->rootBlock = OS::toLeU32(p_curFileHead.rootBlock);
		file_head->numBlocks = OS::toLeU32(p_curFileHead.numBlocks);
		file_head->depth = OS::toLeU32(p_curFileHead.depth);
		p_pageCache.writePage(0);
		p_pageCache.releasePage(0);
	}
};

template<typename KeyType>
Btree<KeyType>::Btree
		(std::string name,
		Linux::size_type block_size,
		Linux::size_type key_size,
		Linux::size_type val_size)
			: p_name(name), p_pageCache(block_size), p_blockSize(block_size),
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
typename Btree<KeyType>::Ref Btree<KeyType>::IterateClosure::position() {
	return Ref(p_block, p_entry);
}
template<typename KeyType>
void Btree<KeyType>::IterateClosure::seek(Ref ref, Async::Callback<void()> callback) {
	p_callback = callback;

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

	blknum_type right_link = p_tree->p_leafGetRightLink(p_buffer);
	if(p_entry == p_tree->p_leafGetEntCount(p_buffer)) {
		if(right_link != 0) {
			p_block = right_link;
			p_entry = 0;
			p_tree->p_pageCache.readPage(right_link,
					ASYNC_MEMBER(this, &IterateClosure::forwardOnRead));
		}else{
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
void Btree<KeyType>::InsertClosure::insert(KeyType *key, void *value,
		UnaryCompareCallback compare,
		Async::Callback<void()> on_complete) {
	p_keyPtr = key;
	p_valuePtr = value;
	p_compare = compare;
	p_onComplete = on_complete;

	p_blockNumber = p_tree->p_curFileHead.rootBlock;
	p_tree->p_pageCache.readPage(p_blockNumber,
			ASYNC_MEMBER(this, &InsertClosure::onReadRoot));
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::onReadRoot(char *buffer) {
	p_blockBuffer = buffer;

	/* check if we have to split the root */
	if(p_tree->blockIsFull(p_blockNumber, p_blockBuffer)) {
		p_splitClosure.split(p_blockNumber, p_blockBuffer, nullptr, 0,
				ASYNC_MEMBER(this, &InsertClosure::onSplitRoot));
	}else{
		descend();
	}
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::onSplitRoot() {
	p_tree->p_pageCache.writePage(p_blockNumber);

	p_blockNumber = p_tree->p_curFileHead.rootBlock;
	p_tree->p_pageCache.readPage(p_blockNumber,
			ASYNC_MEMBER(this, &InsertClosure::onReadRoot));
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::descend() {
	int flags = p_tree->p_headGetFlags(p_blockBuffer);
	if((flags & 1) != 0) {
		assert(p_tree->p_leafGetEntCount(p_blockBuffer) < p_tree->p_entsPerLeaf());
	}else{
		assert(p_tree->p_innerGetEntCount(p_blockBuffer) < p_tree->p_entsPerInner());
	}
	if((flags & 1) != 0) {
		p_searchClosure.prevInLeaf(p_blockBuffer, p_compare,
				ASYNC_MEMBER(this, &InsertClosure::insertIntoNode));
		return;
	}
	
	p_searchClosure.prevInInner(p_blockBuffer, p_compare,
			ASYNC_MEMBER(this, &InsertClosure::onFoundChild));
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::onFoundChild(int index) {
	p_childIndex = index;

	char *ref_ptr = p_blockBuffer + (p_childIndex >= 0
			? p_tree->p_refOffInner(p_childIndex) : p_tree->p_lrefOffInner());
	
	p_childNumber = OS::fromLeU32(*((blknum_type*)ref_ptr));
	p_tree->p_pageCache.readPage(p_childNumber,
			ASYNC_MEMBER(this, &InsertClosure::onReadChild));
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::onReadChild(char *buffer) {
	p_childBuffer = buffer;

	/* check if we have to split the child */
	if(p_tree->blockIsFull(p_childNumber, p_childBuffer)) {
		p_splitClosure.split(p_childNumber, p_childBuffer,
				p_blockBuffer, p_childIndex,
				ASYNC_MEMBER(this, &InsertClosure::onSplitChild));
	}else{
		p_tree->p_pageCache.releasePage(p_blockNumber);
		p_blockBuffer = p_childBuffer;
		p_blockNumber = p_childNumber;
		
		descend();
	}
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::onSplitChild() {
	p_tree->p_pageCache.writePage(p_blockNumber);
	p_tree->p_pageCache.writePage(p_childNumber);
	p_tree->p_pageCache.releasePage(p_childNumber);

	descend();
}
template<typename KeyType>
void Btree<KeyType>::InsertClosure::insertIntoNode(int index) {
	if(index >= 0) {
		p_tree->p_insertAtLeaf(p_blockBuffer, index + 1, *p_keyPtr, p_valuePtr);
	}else{
		p_tree->p_insertAtLeaf(p_blockBuffer, 0, *p_keyPtr, p_valuePtr);
	}
	p_tree->p_pageCache.writePage(p_blockNumber);
	p_tree->p_pageCache.releasePage(p_blockNumber);

	p_onComplete();
}

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
	if((flags & 1) != 0) {
		if(p_tree->p_leafGetEntCount(p_blockBuffer) != 0) {
			Ref result;
			result.block = p_blockNumber;
			result.entry = 0;
			
			p_onComplete(result);
			return;
		}else{
			p_onComplete(Ref());
			return;
		}
	}
	
	char *ref_ptr = p_blockBuffer + p_tree->p_lrefOffInner();
	p_blockNumber = OS::fromLeU32(*((blknum_type*)ref_ptr));

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
	if((flags & 1) != 0) {
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
		blknum_type ent_count = p_tree->p_innerGetEntCount(p_blockBuffer);
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(ent_count - 1);
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		p_blockNumber = child_num;
	}else if(index == 0) {
		char *ref_ptr = p_blockBuffer + p_tree->p_lrefOffInner();
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		p_blockNumber = child_num;
	}else{
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(index - 1);
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
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
	if((flags & 1) != 0) {
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
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		p_blockNumber = child_num;
	}else{
		char *ref_ptr = p_blockBuffer + p_tree->p_refOffInner(index);
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		p_blockNumber = child_num;
	}

	findPrevDescend();
}
template<typename KeyType>
void Btree<KeyType>::FindClosure::findPrevInLeaf(int index) {
	if(index == -1) {
		blknum_type left_link = p_tree->p_leafGetLeftLink(p_blockBuffer);
		if(left_link != 0) {
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

	blknum_type ent_count = p_tree->p_leafGetEntCount(p_blockBuffer);
	assert(ent_count > 0);

	p_tree->p_pageCache.releasePage(p_blockNumber);
	p_onComplete(Ref(p_blockNumber, ent_count - 1));
}

/* ------------------------------------------------------------------------- *
 * NODE SPLITTING FUNCTIONS                                                  *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
bool Btree<KeyType>::blockIsFull(blknum_type block_num, char *block_buf) {
	flags_type flags = OS::fromLeU32(*((flags_type*)block_buf));
	if((flags & 1) != 0) {
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
void Btree<KeyType>::SplitClosure::split(blknum_type block_num, char *block_buf,
		char *parent_buf, blknum_type block_index,
		Async::Callback<void()> on_complete) {
	p_blockNumber = block_num;
	p_blockBuffer = block_buf;
	p_parentBuffer = parent_buf;
	p_blockIndex = block_index;
	p_onComplete = on_complete;

	flags_type flags = OS::fromLeU32(*((flags_type*)block_buf));
	if((flags & 1) != 0) {
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

	blknum_type ent_count = p_tree->p_leafGetEntCount(p_blockBuffer);
	p_rightLinkNumber = p_tree->p_leafGetRightLink(p_blockBuffer);
	int left_n = ent_count / 2;
	int right_n = ent_count - left_n;
	assert(left_n > 0 && right_n > 0
			&& left_n < p_tree->p_entsPerLeaf() - 1
			&& right_n < p_tree->p_entsPerLeaf() - 1);
	p_tree->p_leafSetEntCount(p_blockBuffer, left_n);
	p_tree->p_leafSetRightLink(p_blockBuffer, p_splitNumber);
//		std::cout << "split leaf into: " << left_n << ", " << right_n << std::endl;
	
	char *split_block = p_tree->p_pageCache.initializePage(p_splitNumber);
	p_tree->p_headSetFlags(split_block, 1);
	p_tree->p_leafSetEntCount(split_block, right_n);
	p_tree->p_leafSetLeftLink(split_block, p_blockNumber);
	p_tree->p_leafSetRightLink(split_block, p_rightLinkNumber);

	/* setup the entries of the new block */
	p_splitKey = p_tree->p_readKey(p_blockBuffer + p_tree->p_keyOffLeaf(left_n - 1));
	std::memcpy(split_block + p_tree->p_entOffLeaf(0),
			p_blockBuffer + p_tree->p_entOffLeaf(left_n),
			right_n * p_tree->p_entSizeLeaf());
	/* FIXME: for debugging only! */
	std::memset(p_blockBuffer + p_tree->p_entOffLeaf(left_n), 0, right_n * p_tree->p_entSizeLeaf());
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

	/* note: left_n + right_n = ent_count - 1
		because one of the keys moves to the parent node */
	blknum_type ent_count = p_tree->p_innerGetEntCount(p_blockBuffer);
	int left_n = ent_count / 2;
	int right_n = ent_count - left_n - 1;
	assert(left_n > 0 && right_n > 0
			&& left_n < p_tree->p_entsPerInner() - 2
			&& right_n < p_tree->p_entsPerInner() - 1);
	p_tree->p_innerSetEntCount(p_blockBuffer, left_n);
//		std::cout << "split inner into: " << left_n << ", " << right_n << std::endl;
	
	char *split_block = p_tree->p_pageCache.initializePage(p_splitNumber);
	p_tree->p_headSetFlags(split_block, 0);
	p_tree->p_innerSetEntCount(split_block, right_n);	
	
	/* setup the leftmost child reference of the new block */
	char *block_rref = p_blockBuffer + p_tree->p_refOffInner(left_n);
	char *split_lref = split_block + p_tree->p_lrefOffInner();
	*(blknum_type*)split_lref = *(blknum_type*)block_rref;
	p_splitKey = p_tree->p_readKey(p_blockBuffer + p_tree->p_keyOffInner(left_n));

	/* setup the remaining entries of the new block */
	std::memcpy(split_block + p_tree->p_entOffInner(0),
			p_blockBuffer + p_tree->p_entOffInner(left_n + 1),
			right_n * p_tree->p_entSizeInner());
	/* FIXME: for debugging only! */
	std::memset(p_blockBuffer + p_tree->p_entOffInner(left_n), 0, (right_n + 1) * p_tree->p_entSizeInner());
	p_tree->p_pageCache.writePage(p_splitNumber);
	p_tree->p_pageCache.releasePage(p_splitNumber);
	
	fixParent();
}
template<typename KeyType>
void Btree<KeyType>::SplitClosure::fixParent() {
	if(p_parentBuffer != nullptr) {
		blknum_type parent_count = p_tree->p_innerGetEntCount(p_parentBuffer);
		assert(parent_count < p_tree->p_entsPerInner());

		p_tree->p_insertAtInnerR(p_parentBuffer, p_blockIndex + 1, p_splitKey, p_splitNumber);
	}else{
		blknum_type new_root_num = p_tree->p_allocBlock();
		p_tree->p_curFileHead.rootBlock = new_root_num;
		p_tree->p_curFileHead.depth++;
		p_tree->p_writeFileHead();
		
		char *new_root_buffer = p_tree->p_pageCache.initializePage(new_root_num);
		p_tree->p_headSetFlags(new_root_buffer, 0);
		p_tree->p_innerSetEntCount(new_root_buffer, 1);
		
		char *lref = new_root_buffer + p_tree->p_lrefOffInner();
		char *ref0 = new_root_buffer + p_tree->p_refOffInner(0);
		*((blknum_type*)lref) = OS::toLeU32(p_blockNumber);
		*((blknum_type*)ref0) = OS::toLeU32(p_splitNumber);
		p_tree->p_writeKey(new_root_buffer + p_tree->p_keyOffInner(0), p_splitKey);

		p_tree->p_pageCache.writePage(new_root_num);
		p_tree->p_pageCache.releasePage(new_root_num);
	}

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
void Btree<KeyType>::p_insertAtLeaf(char *block, blknum_type i,
		const KeyType &key, void *value) {
	blknum_type ent_count = p_leafGetEntCount(block);
	std::memmove(block + p_entOffLeaf(i + 1), block + p_entOffLeaf(i),
		(ent_count - i) * p_entSizeLeaf());
	p_writeKey(block + p_keyOffLeaf(i), key);
	std::memcpy(block + p_valOffLeaf(i), value, p_valSize);
	p_leafSetEntCount(block, ent_count + 1);
}
template<typename KeyType>
void Btree<KeyType>::p_insertAtInnerR(char *block, int i,
		const KeyType &key, blknum_type ref) {
	blknum_type ent_count = p_innerGetEntCount(block);
	std::memmove(block + p_entOffInner(i + 1), block + p_entOffInner(i),
		(ent_count - i) * p_entSizeInner());
	p_writeKey(block + p_keyOffInner(i), key);
	char *refi = block + p_refOffInner(i);
	*((blknum_type*)refi) = ref;
	p_innerSetEntCount(block, ent_count + 1);
}

/* ------------------------------------------------------------------------- *
 * DEBUGGING FUNCTIONS                                                       *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::p_blockIntegrity(blknum_type block_num,
		KeyType min, KeyType max) {
	char *block_buf = new char[p_blockSize];
	//FIXME: use async read instead
	//p_readBlock(block_num, block_buf);
	
	flags_type flags = OS::fromLeU32(*(flags_type*)block_buf);
	if((flags & 1) != 0) {
		blknum_type ent_count = p_innerGetEntCount(block_buf);
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
		blknum_type ent_count = p_innerGetEntCount(block_buf);
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
		p_blockIntegrity(*(blknum_type*)lref, min,
				p_readKey(block_buf + p_keyOffInner(0)));
		for(int i = 0; i < ent_count; i++) {
			KeyType ckey = p_readKey(block_buf + p_keyOffInner(i));
			KeyType nkey = (i == ent_count - 1 ? max
				: p_readKey(block_buf + p_keyOffInner(i + 1)));
			char *refi = block_buf + p_refOffInner(i);
			p_blockIntegrity(*(blknum_type*)refi, ckey, nkey);
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
typename Btree<KeyType>::blknum_type Btree<KeyType>::p_innerGetEntCount(char *block_buf) {
	InnerHead *head = (InnerHead*)block_buf;
	return OS::fromLeU32(head->entCount2);
}
template<typename KeyType>
void Btree<KeyType>::p_innerSetEntCount(char *block_buf,
		typename Btree<KeyType>::blknum_type ent_count) {
	InnerHead *head = (InnerHead*)block_buf;
	head->entCount2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::blknum_type Btree<KeyType>::p_leafGetEntCount(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->entCount2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetEntCount(char *block_buf,
		typename Btree<KeyType>::blknum_type ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->entCount2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::blknum_type Btree<KeyType>::p_leafGetLeftLink(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->leftLink2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetLeftLink(char *block_buf,
		typename Btree<KeyType>::blknum_type ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->leftLink2 = OS::toLeU32(ent_count);
}

template<typename KeyType>
typename Btree<KeyType>::blknum_type Btree<KeyType>::p_leafGetRightLink(char *block_buf) {
	LeafHead *head = (LeafHead*)block_buf;
	return OS::fromLeU32(head->rightLink2);
}
template<typename KeyType>
void Btree<KeyType>::p_leafSetRightLink(char *block_buf,
		typename Btree<KeyType>::blknum_type ent_count) {
	LeafHead *head = (LeafHead*)block_buf;
	head->rightLink2 = OS::toLeU32(ent_count);
}

#endif

