#include "PageCache.h"

PageCache PageCache::_sInst;

//给CentralCache k页大小的Span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	//处理>128页的大内存申请,向堆申请
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		//Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageID = (PAGE_ID)(ptr) >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap[span->_pageID] = span;

		return span;
	}

	//先检查page cache第k个桶有没有span
	if (!_spanLists[k].IsEmpty()) {

		Span* kSpan = _spanLists[k].PopFront();

		//建立page_id和span的映射，方便central cache回收小块内存时，查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_n; i++) {
			_idSpanMap[kSpan->_pageID + i] = kSpan;
		}

		//返回k页的span
		return kSpan;
	}

	//再检查一下后面的桶有没有span，如果有，则可以进行切分
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanLists[i].IsEmpty()) {
			//开始切分,将i页的span切分成k页的span和一个i-k页的span
			//k页的span返回给central cache
			//i-k页的span挂到第i-k个桶中去
			Span* iSpan = _spanLists[i].PopFront();
			//Span* kSpan = new Span;
			Span* kSpan = _spanPool.New();

			//在iSpan的头部切一个k页下来
			kSpan->_pageID = iSpan->_pageID;
			kSpan->_n = k;

			iSpan->_pageID += k;
			iSpan->_n -= k;


			//将i-k页的span挂到第i-k个桶中去
			_spanLists[iSpan->_n].PushFront(iSpan);
			//存储iSpan的首位页号和跟iSpan的映射，方便pagecache回收	内存时进行合并查找
			_idSpanMap[iSpan->_pageID] = iSpan;
			_idSpanMap[iSpan->_pageID + iSpan->_n - 1] = iSpan;


			//建立page_id和span的映射，方便central cache回收小块内存时，查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; i++) {
				_idSpanMap[kSpan->_pageID + i] = kSpan;
			}

			//返回k页的span
			return kSpan;
		}
	}

	//走到这一步就说明后面找不到大页的span去切分了
	//这时就去找堆要一个128页的span
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageID = (PAGE_ID)ptr >> 13;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}

//获取内存块和Span的映射
Span* PageCache::MapObjectToSpan(void* obj) {
	//通过内存块的地址计算所在页号，然后通过page_id和span的映射Map即可知道内存块在哪个span
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);

	std::unique_lock<std::mutex>lock(_pageMtx);
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end()) {
		return ret->second;
	}
	else {
		assert(false);//不可能找不到
		return nullptr;
	}
}

//将central cache的span归还给page cache
void PageCache::ReleaseSpanToPageCache(Span* span) {

	if (span->_n > NPAGES - 1) {//说明是找堆要的大内存
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}


	//对span前后的页尝试进行合并，缓解内存碎片问题
	//向前合并
	while (true) {
		PAGE_ID prevId = span->_pageID - 1;
		auto ret = _idSpanMap.find(prevId);
		//1.前面页号没有，不合并了
		if (ret == _idSpanMap.end()) {
			break;
		}
		Span* prevSpan = ret->second;
		//2.前面相邻页的span在使用，不合并了
		if (prevSpan->_isUsed == true) {
			break;
		}
		//3.合并出超过128页（NPAGES-1）的span没办法管理，也不合并
		if (prevSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		//4.合并
		span->_pageID = prevSpan->_pageID;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	//向后合并
	while (true) {
		PAGE_ID nextID = span->_pageID + span->_n;
		auto ret = _idSpanMap.find(nextID);
		//1.后面页号没有，不合并了
		if (ret == _idSpanMap.end()) {
			break;
		}
		Span* nextSpan = ret->second;
		//2.后面相邻页的span在使用，不合并了
		if (nextSpan->_isUsed == true) {
			break;
		}
		//3.合并出超过128页（NPAGES-1）的span没办法管理，也不合并
		if (nextSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		//4.合并
		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);

		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	//合并后将span挂起来
	_spanLists[span->_n].PushFront(span);
	span->_isUsed = false;
	_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_n - 1] = span;

}