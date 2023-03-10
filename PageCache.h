#pragma once
#include "Common.h"

//全局只有一个PageCache，因此可以使用单例饿汉模式
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInst;
	}

	//给CentralCache k页大小的Span
	Span* NewSpan(size_t k);

	//获取内存块和Span的映射
	Span* MapObjectToSpan(void* obj);

	//将central cache的span归还给page cache
	void ReleaseSpanToPageCache(Span* span);
public:
	std::mutex _pageMtx;//page cache的锁不是桶锁 而是全局锁，因为牵扯到页的分裂(切割)和合并问题。
private:

	SpanList _spanLists[NPAGES];
	//页号与span的映射
	std::unordered_map<PAGE_ID, Span*>_idSpanMap;
	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};