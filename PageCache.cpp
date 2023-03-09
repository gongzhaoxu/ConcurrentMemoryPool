#include "PageCache.h"

PageCache PageCache::_sInst;

//给CentralCache k页大小的Span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0 && k < NPAGES);

	//先检查page cache第k个桶有没有span
	if (!_spanLists[k].IsEmpty()) {
		return _spanLists->PopFront();
	}

	//再检查一下后面的桶有没有span，如果有，则可以进行切分
	for (size_t i = k + 1; k < NPAGES; i++) {
		if (!_spanLists[i].IsEmpty()) {
			//开始切分,将i页的span切分成k页的span和一个i-k页的span
			//k页的span返回给central cache
			//i-k页的span挂到第i-k个桶中去
			Span* iSpan = _spanLists[i].PopFront();
			Span* kSpan = new Span;

			//在iSpan的头部切一个k页下来
			kSpan->_pageID = iSpan->_pageID;
			kSpan->_n = k;

			iSpan->_pageID += k;
			iSpan->_n -= k;


			//将i-k页的span挂到第i-k个桶中去
			_spanLists[iSpan->_n].PushFront(iSpan);

			//返回k页的span
			return kSpan;
		}
	}

	//走到这一步就说明后面找不到大页的span去切分了
	//这时就去找堆要一个128页的span
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageID = (PAGE_ID)ptr >> 13;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k); 
}