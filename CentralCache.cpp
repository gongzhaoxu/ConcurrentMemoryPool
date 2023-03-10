#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

//从中心缓存获取一个非空的Span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	//先查看当前的spanlist中是否有未分配对象的span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList) {//只要当前Span下的_freeList不为空
			return it;
		}
		else {
			it = it->_next;
		}
	}


	//先把central cache的桶锁解了，这样如果其他进程释放内存回来不会阻塞
	list._mtx.unlock();

	//走到这里说明没有空闲span了，只能找page cache要内存,要一个span
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeCLass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();
	//从NewSpan()函数出来解锁page cache的锁后，不需要给central cache加桶锁
	//因为其他线程拿不到这个span
	

	//span的大块内存起始地址、结束地址
	char* startAddress = (char*)(span->_pageID << PAGE_SHIFT);
	
	//span的大块内存字节数
	size_t bytes = span->_n << PAGE_SHIFT;
	char* endAddress = startAddress + bytes;

	//把大块内存切成自由链表连接起来
	//1.先切一块下来做头，方便尾插
	span->_freeList = startAddress;
	startAddress += size;
	//2.尾插
	void* tail = span->_freeList;
	
	while (startAddress < endAddress) {
		NextObj(tail) = startAddress;
		tail = NextObj(tail);
		startAddress += size;
	}

	//切好span以后，需要把span挂到桶里面去的时候再加锁
	list._mtx.lock();
	list.PushFront(span);
	return span;
}


//从中心缓存获取一定数量的对象给thread cache
// central cache和 thread cache映射关系一致。
//batchNum为申请的个数，size为单个内存大小
//若central cache的span挂有batchNum个内存块，则分给batchNum个，没有这么多则 至少 给一个。
size_t CentralCache::FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeCLass::Index(size);
	_spanLists[index]._mtx.lock();//加上桶锁

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = start;

	//从span中获取batchNum个对象
	//若不够batchNum个，有多少拿多少

	size_t i = 0;
	size_t actualNum = 1;//注意actualNum初始值为1而不是0
	while (i < batchNum - 1 && NextObj(end) != nullptr) {
		end = NextObj(end);
		i++;
		actualNum++;
	}

	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	_spanLists[index]._mtx.unlock();//解桶锁

	return actualNum;
}


//将一定数量的内存块释放到Span中
void CentralCache::ReleaseListToSpans(void* start, size_t size) {

}