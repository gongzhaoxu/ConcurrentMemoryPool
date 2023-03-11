#pragma once
#include"Common.h"
#include"ObjectPool.h"

//多层基数树若全开满相比单层所占内存空间一致，但其不用一次开满，需要时再开辟数组
//3层适合64位机

//建立一个二层的基数树，原理与页表类似
//BITS本质上是页号要占用的位数
//32位机下 BITS=32-PAGE_SHIFT=19  
//64位机下 BITS=64-PAGE_SHIFT=51

template<int BITS>
class TcMalloc_PageMap2
{
private:
	//在根中放入32个条目，在每个叶中放入（2^ BITS）/ 32个条目。
	static const int ROOT_BITS = 5;
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	struct Leaf {
		void* values[LEAF_LENGTH];
	};
	Leaf* _root[ROOT_LENGTH];
public:
	typedef PAGE_ID Number;
	explicit TcMalloc_PageMap2()
	{
		memset(_root, 0, sizeof(_root));
		PreallocateMoreMemory();
	}

	void Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1; )
		{
			const Number i1 = key >> LEAF_BITS;
			//检查是否越界
			if (i1 >= ROOT_LENGTH) return;
			//如果有必要，创建第二层
			if (_root[i1] == nullptr)
			{
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = (Leaf*)(leafPool.New());
				memset(leaf, 0, sizeof(*leaf));
				_root[i1] = leaf;
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
	}

	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS);
	}

	void* get(Number k)const
	{
		//在根结点中的索引
		const Number i1 = k >> LEAF_BITS;
		//在叶子结点中的位置
		const Number i2 = k & (LEAF_LENGTH - 1);
		//越界或者根结点为空
		if ((k >> BITS) > 0 || _root[i1] == nullptr)
			return nullptr;
		return _root[i1]->values[i2];
	}
	void Set(Number k, void* v)
	{
		//在根结点中的索引
		const Number i1 = k >> LEAF_BITS;
		//在叶子结点中的位置
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		_root[i1]->values[i2] = v;
	}
};