
template<typename KeyType>
class Btree {
public:
	typedef int32_t blknum_type;

	typedef std::function<int(KeyType, KeyType)> Compare;
	typedef std::function<void(void*, KeyType)> WriteKey;
	typedef std::function<KeyType(const void*)> ReadKey;

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

	// FIXME: Seq should properly deallocate memory
	class Seq {
	friend class Btree;
	public:
		~Seq() {
			delete[] p_buffer;
		}
		Seq() : p_tree(nullptr), p_buffer(nullptr), p_block(0), p_entry(0) { }
		Seq(Seq &&other) :
				p_tree(other.p_tree), p_buffer(other.p_buffer),
				p_block(other.p_block), p_entry(other.p_entry) {
		}

		void operator= (Seq &&other) {
			p_tree = other.p_tree;
			p_buffer = other.p_buffer;
			p_block = other.p_block;
			p_entry = other.p_entry;
			
			other.p_buffer = nullptr;
			other.p_block = 0;
			other.p_entry = 0;
		}
		Seq(const Seq &other) = delete;
		
		Ref position() {
			return Ref(p_block, p_entry);
		}
		
		bool valid() const {
			return p_block > 0 && p_entry >= 0;
		}
		
		void operator++ ();
		KeyType getKey();
		void getValue(void *value);
		void setValue(void *value);
		
	private:
		Seq(Btree *tree, blknum_type block, blknum_type entry)
				: p_tree(tree), p_block(block), p_entry(entry) {
			p_buffer = new char[tree->p_blockSize];
		}
		
		Btree *p_tree;
		char *p_buffer;
		blknum_type p_block;
		blknum_type p_entry;
	};

	void setPath(const std::string &path) {
		p_path = path;
	}
	std::string getPath() {
		return p_path;
	}
	
	void setCompare(const Compare &compare) {
		p_compare = compare;
	}
	void setWriteKey(const WriteKey &write_key) {
		p_writeKey = write_key;
	}
	void setReadKey(const ReadKey &read_key) {
		p_readKey = read_key;
	}

	void createTree() {
		p_file->openSync(p_path + "/" + p_name + ".btree",
				Linux::kFileCreate | Linux::kFileTrunc
				| Linux::FileMode::read | Linux::FileMode::write);

		p_curFileHead.rootBlock = 1;
		p_curFileHead.numBlocks = 2;
		p_curFileHead.depth = 1;
		p_writeFileHead();

		char *root_block = new char[p_blockSize];
		p_headSetFlags(root_block, 1);
		p_leafSetEntCount(root_block, 0);
		p_leafSetLeftLink(root_block, 0);
		p_leafSetRightLink(root_block, 0);
		p_file->pwriteSync(1 * p_blockSize, p_blockSize, root_block);
		delete[] root_block;
	}

	void loadTree() {
		p_file->openSync(p_path + "/" + p_name + ".btree",
				Linux::FileMode::read | Linux::FileMode::write);
		
		char *desc_block = new char[p_blockSize];
		p_file->preadSync(0 * p_blockSize, p_blockSize, desc_block);
		FileHead *file_head = (FileHead*)desc_block;
		p_curFileHead.rootBlock = OS::fromLeU32(file_head->rootBlock);
		p_curFileHead.numBlocks = OS::fromLeU32(file_head->numBlocks);
		p_curFileHead.depth = OS::fromLeU32(file_head->depth);
		delete[] desc_block;
	}
	
	void closeTree() {
		p_file->closeSync();
	}
	
	void integrity(KeyType min, KeyType max) {
		p_blockIntegrity(p_curFileHead.rootBlock, min, max);
	}

	void insert(const KeyType &key, void *value,
			std::function<int(const KeyType&)> compare);
	Ref findNext(std::function<int(const KeyType&)> compare,
			KeyType *found, void *value);
	Ref refToFirst();
	
	Seq sequence(const Ref &ref) {
		Seq result(this, ref.block, ref.entry);
		if(ref.block > 0)
			p_readBlock(ref.block, result.p_buffer);
		return result;
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

	Compare p_compare;
	ReadKey p_readKey;
	WriteKey p_writeKey;

	std::string p_path;
	std::string p_name;
	std::unique_ptr<Linux::File> p_file;
	
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
	
	int p_prevInLeaf(char *block,
			std::function<int(const KeyType&)> compare);
	int p_prevInInner(char *block,
			std::function<int(const KeyType&)> compare);
	int p_nextInLeaf(char *block,
			std::function<int(const KeyType&)> compare);
	int p_nextInInner(char *block,
			std::function<int(const KeyType&)> compare);

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

	bool p_checkSplit(blknum_type block_num, char *block_buf,
			char *parent_buf, blknum_type parent_i);
	void p_fixParent(char *parent_buf, int parent_i, KeyType &split_key,
			blknum_type block_num, blknum_type split_num);
	void p_splitLeaf(char *block, blknum_type blk_num,
			char *parent_buf, int parent_i);
	void p_splitInner(char *block, blknum_type blk_num,
			char *parent_buf, int parent_i);

	void p_blockIntegrity(blknum_type block_num,
			KeyType min, KeyType max);
	
	void p_readBlock(blknum_type block_num, char *block_buf) {
		p_file->preadSync(block_num * p_blockSize, p_blockSize, block_buf);
	}
	void p_writeBlock(blknum_type block_num, char *block_buf) {
		p_file->pwriteSync(block_num * p_blockSize, p_blockSize, block_buf);
	}
	void p_writeFileHead() {
		char *desc_block = new char[p_blockSize];
		FileHead *file_head = (FileHead*)desc_block;
		file_head->rootBlock = OS::toLeU32(p_curFileHead.rootBlock);
		file_head->numBlocks = OS::toLeU32(p_curFileHead.numBlocks);
		file_head->depth = OS::toLeU32(p_curFileHead.depth);
		p_writeBlock(0, desc_block);
		delete[] desc_block;
	}
};

template<typename KeyType>
Btree<KeyType>::Btree
		(std::string name,
		Linux::size_type block_size,
		Linux::size_type key_size,
		Linux::size_type val_size)
			: p_name(name), p_blockSize(block_size),
		p_keySize(key_size), p_valSize(val_size) {
	assert(p_blockSize > sizeof(FileHead)
			&& p_blockSize > sizeof(InnerHead)
			&& p_blockSize > sizeof(LeafHead));
	assert(p_entsPerInner() >= 4);
	assert(p_entsPerLeaf() >= 4);

//	std::cout << "ents per inner: " << p_entsPerInner()
//			<< ", leaf: " << p_entsPerLeaf() << std::endl;

	p_file = osIntf->createFile();
}

/* ------------------------------------------------------------------------- *
 * REF AND SEQUENCE FUNCTIONS                                                 *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::Seq::operator++() {
	assert(p_block > 0 && p_entry < p_tree->p_leafGetEntCount(p_buffer));
	p_entry++;

	blknum_type right_link = p_tree->p_leafGetRightLink(p_buffer);
	if(p_entry == p_tree->p_leafGetEntCount(p_buffer)) {
		if(right_link != 0) {
			p_tree->p_readBlock(right_link, p_buffer);
			p_block = right_link;
			p_entry = 0;
		}else{
			p_block = -1;
			p_entry = -1;
		}
	}
}

template<typename KeyType>
KeyType Btree<KeyType>::Seq::getKey() {
	assert(p_block > 0 && p_entry >= 0);
	return p_tree->p_readKey(p_buffer + p_tree->p_keyOffLeaf(p_entry));
}

template<typename KeyType>
void Btree<KeyType>::Seq::getValue(void *value) {
	assert(p_block > 0 && p_entry >= 0);
	std::memcpy(value, p_buffer + p_tree->p_valOffLeaf(p_entry),
			p_tree->p_valSize);
}
template<typename KeyType>
void Btree<KeyType>::Seq::setValue(void *value) {
	assert(p_block > 0 && p_entry >= 0);
	std::memcpy(p_buffer + p_tree->p_valOffLeaf(p_entry), value,
			p_tree->p_valSize);
	p_tree->p_writeBlock(p_block, p_buffer);
}

/* ------------------------------------------------------------------------- *
 * INSERT AND FIND FUNCTIONS                                                 *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
void Btree<KeyType>::insert(const KeyType &key, void *value,
		std::function<int(const KeyType&)> compare) {
//		std::cout << "insert: " << key << std::endl;
	blknum_type block_num = p_curFileHead.rootBlock;
	char *block_buf = new char[p_blockSize];
	p_readBlock(block_num, block_buf);

	/* check if we have to split the root */
	if(p_checkSplit(block_num, block_buf, nullptr, 0)) {
		p_writeBlock(block_num, block_buf);

		block_num = p_curFileHead.rootBlock;
		p_readBlock(block_num, block_buf);
	}
	
	while(true) {
		int flags = p_headGetFlags(block_buf);
		if((flags & 1) != 0) {
			assert(p_leafGetEntCount(block_buf) < p_entsPerLeaf());
		}else{
			assert(p_innerGetEntCount(block_buf) < p_entsPerInner());
		}
		if((flags & 1) != 0) {
			int i = p_prevInLeaf(block_buf, compare);
			if(i >= 0) {
				p_insertAtLeaf(block_buf, i + 1, key, value);
			}else{
				p_insertAtLeaf(block_buf, 0, key, value);
			}
			p_writeBlock(block_num, block_buf);
			delete[] block_buf;
			return;
		}
		
		int i = p_prevInInner(block_buf, compare);
		char *ref_ptr = block_buf
				+ (i >= 0 ? p_refOffInner(i) : p_lrefOffInner());
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		
		char *child_buf = new char[p_blockSize];
		p_readBlock(child_num, child_buf);
		
		/* check if we have to split the child */
		if(p_checkSplit(child_num, child_buf, block_buf, i)) {
			p_writeBlock(block_num, block_buf);
			p_writeBlock(child_num, child_buf);
			delete[] child_buf;
		}else{
			delete[] block_buf;
			block_buf = child_buf;
			block_num = child_num;
		}
	}
}

template<typename KeyType>
typename Btree<KeyType>::Ref Btree<KeyType>::findNext(
		std::function<int(const KeyType&)> compare,
		KeyType *found, void *value) {
//std::cout << p_entsPerLeaf() << std::endl;
	blknum_type block_num = p_curFileHead.rootBlock;
	char *block_buf = new char[p_blockSize];
	while(true) {
		p_readBlock(block_num, block_buf);
		
		flags_type flags = p_headGetFlags(block_buf);
		if((flags & 1) != 0) {
			int i = p_nextInLeaf(block_buf, compare);
			if(i >= 0) {
				if(found != nullptr)
					*found = p_readKey(block_buf + p_keyOffLeaf(i));
				if(value != nullptr)
					std::memcpy(value, block_buf + p_valOffLeaf(i), p_valSize);
				delete[] block_buf;
				return Ref(block_num, i);
			}else{
				delete[] block_buf;
				return Ref(-1, -1);
			}
		}

		blknum_type ent_count = p_innerGetEntCount(block_buf);
		int i = p_nextInInner(block_buf, compare);
		int j = (i == -1 ? ent_count : i);
		char *ref_ptr = block_buf
				+ (j > 0 ? p_refOffInner(j - 1) : p_lrefOffInner());
		blknum_type child_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
		block_num = child_num;
	}
}

template<typename KeyType>
typename Btree<KeyType>::Ref Btree<KeyType>::refToFirst() {
	blknum_type block_num = p_curFileHead.rootBlock;
	
	char *block_buf = new char[p_blockSize];
	while(true) {
		p_readBlock(block_num, block_buf);
		
		flags_type flags = p_headGetFlags(block_buf);
		if((flags & 1) != 0) {
			if(p_leafGetEntCount(block_buf) != 0) {
				Ref result;
				result.block = block_num;
				result.entry = 0;
				return result;
			}else{
				return Ref();
			}
		}
		
		char *ref_ptr = block_buf + p_lrefOffInner();
		block_num = OS::fromLeU32(*((blknum_type*)ref_ptr));
	}
}

/* ------------------------------------------------------------------------- *
 * NODE SPLITTING FUNCTIONS                                                  *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
bool Btree<KeyType>::p_checkSplit(blknum_type block_num, char *block_buf,
		char *parent_buf, blknum_type parent_i) {
	flags_type flags = OS::fromLeU32(*((flags_type*)block_buf));
	if((flags & 1) != 0) {
		if(p_leafGetEntCount(block_buf) >= p_entsPerLeaf() - 1) {
			p_splitLeaf(block_buf, block_num, parent_buf, parent_i);
			return true;
		}
	}else{
		if(p_innerGetEntCount(block_buf) >= p_entsPerInner() - 1) {
			p_splitInner(block_buf, block_num, parent_buf, parent_i);
			return true;
		}
	}
	return false;
}

template<typename KeyType>
void Btree<KeyType>::p_splitLeaf(char *block, blknum_type blk_num,
		char *parent_buf, int parent_i) {
//	std::cout << "split leaf" << std::endl;
	blknum_type split_num = p_allocBlock();

	blknum_type ent_count = p_leafGetEntCount(block);
	blknum_type right_link = p_leafGetRightLink(block);
	int left_n = ent_count / 2;
	int right_n = ent_count - left_n;
	assert(left_n > 0 && right_n > 0
			&& left_n < p_entsPerLeaf() - 1
			&& right_n < p_entsPerLeaf() - 1);
	p_leafSetEntCount(block, left_n);
	p_leafSetRightLink(block, split_num);
//		std::cout << "split leaf into: " << left_n << ", " << right_n << std::endl;
	
	char *split_block = new char[p_blockSize];
	p_headSetFlags(split_block, 1);
	p_leafSetEntCount(split_block, right_n);
	p_leafSetLeftLink(split_block, blk_num);
	p_leafSetRightLink(split_block, right_link);

	if(right_link != 0) {
		char *right_buf = new char[p_blockSize];
		p_readBlock(right_link, right_buf);
		p_leafSetLeftLink(right_buf, split_num);
		p_writeBlock(right_link, right_buf);
		delete[] right_buf;
	}

	/* setup the entries of the new block */
	KeyType split_key = p_readKey(block + p_keyOffLeaf(left_n - 1));
	std::memcpy(split_block + p_entOffLeaf(0),
			block + p_entOffLeaf(left_n),
			right_n * p_entSizeLeaf());
	/* FIXME: for debugging only! */
	std::memset(block + p_entOffLeaf(left_n), 0, right_n * p_entSizeLeaf());
	p_writeBlock(split_num, split_block);
	delete[] split_block;
	
	p_fixParent(parent_buf, parent_i, split_key, blk_num, split_num);
}

template<typename KeyType>
void Btree<KeyType>::p_splitInner(char *block, blknum_type blk_num,
		char *parent_buf, int parent_i) {
//	std::cout << "split inner" << std::endl;
	blknum_type split_num = p_allocBlock();

	/* note: left_n + right_n = ent_count - 1
		because one of the keys moves to the parent node */
	blknum_type ent_count = p_innerGetEntCount(block);
	int left_n = ent_count / 2;
	int right_n = ent_count - left_n - 1;
	assert(left_n > 0 && right_n > 0
			&& left_n < p_entsPerInner() - 2
			&& right_n < p_entsPerInner() - 1);
	p_innerSetEntCount(block, left_n);
//		std::cout << "split inner into: " << left_n << ", " << right_n << std::endl;
	
	char *split_block = new char[p_blockSize];
	p_headSetFlags(split_block, 0);
	p_innerSetEntCount(split_block, right_n);	
	
	/* setup the leftmost child reference of the new block */
	char *block_rref = block + p_refOffInner(left_n);
	char *split_lref = split_block + p_lrefOffInner();
	*(blknum_type*)split_lref = *(blknum_type*)block_rref;
	KeyType split_key = p_readKey(block + p_keyOffInner(left_n));

	/* setup the remaining entries of the new block */
	std::memcpy(split_block + p_entOffInner(0),
			block + p_entOffInner(left_n + 1),
			right_n * p_entSizeInner());
	/* FIXME: for debugging only! */
	std::memset(block + p_entOffInner(left_n), 0, (right_n + 1) * p_entSizeInner());
	p_writeBlock(split_num, split_block);
	delete[] split_block;
	
	p_fixParent(parent_buf, parent_i, split_key, blk_num, split_num);
}

template<typename KeyType>
void Btree<KeyType>::p_fixParent(char *parent_buf, int parent_i,
		KeyType &split_key, blknum_type block_num, blknum_type split_num) {
	if(parent_buf != nullptr) {
		blknum_type parent_count = p_innerGetEntCount(parent_buf);
		assert(parent_count < p_entsPerInner());

		p_insertAtInnerR(parent_buf, parent_i + 1, split_key, split_num);
	}else{
		blknum_type root_num = p_allocBlock();
		p_curFileHead.rootBlock = root_num;
		p_curFileHead.depth++;
		p_writeFileHead();
		
		char *root_block = new char[p_blockSize];
		p_headSetFlags(root_block, 0);
		p_innerSetEntCount(root_block, 1);
		
		char *lref = root_block + p_lrefOffInner();
		char *ref0 = root_block + p_refOffInner(0);
		*((blknum_type*)lref) = OS::toLeU32(block_num);
		*((blknum_type*)ref0) = OS::toLeU32(split_num);
		p_writeKey(root_block + p_keyOffInner(0), split_key);

		p_writeBlock(root_num, root_block);
		delete[] root_block;
	}
}

/* ------------------------------------------------------------------------- *
 * INTERNAL UTILITY FUNCTIONS                                                *
 * ------------------------------------------------------------------------- */

template<typename KeyType>
int Btree<KeyType>::p_prevInLeaf(char *block,
		std::function<int(const KeyType&)> compare) {
	auto ent_count = p_leafGetEntCount(block);
	for(blknum_type i = ent_count; i > 0; i--) {
		KeyType ent_key = p_readKey(block + p_keyOffLeaf(i - 1));
		if(compare(ent_key) <= 0)
			return i - 1;
	}
	return -1;
}
template<typename KeyType>
int Btree<KeyType>::p_prevInInner(char *block,
		std::function<int(const KeyType&)> compare) {
	auto ent_count = p_innerGetEntCount(block);
	for(blknum_type i = ent_count; i > 0; i--) {
		KeyType ent_key = p_readKey(block + p_keyOffInner(i - 1));
		if(compare(ent_key) <= 0)
			return i - 1;
	}
	return -1;
}

template<typename KeyType>
int Btree<KeyType>::p_nextInLeaf(char *block,
		std::function<int(const KeyType&)> compare) {
	auto ent_count = p_leafGetEntCount(block);
	for(blknum_type i = 0; i < ent_count; i++) {
		KeyType ent_key = p_readKey(block + p_keyOffLeaf(i));
		if(compare(ent_key) >= 0) {
			return i;
		}
	}
	return -1;
}
template<typename KeyType>
int Btree<KeyType>::p_nextInInner(char *block,
		std::function<int(const KeyType&)> compare) {
	auto ent_count = p_innerGetEntCount(block);
	for(blknum_type i = 0; i < ent_count; i++) {
		KeyType ent_key = p_readKey(block + p_keyOffInner(i));
//std::cout << "c[" << i << "/" << ent_count << "]: " << compare(ent_key) << std::endl;
		if(compare(ent_key) >= 0)
			return i;
	}
	return -1;
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
	p_readBlock(block_num, block_buf);
	
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
	delete[] block_buf;
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

