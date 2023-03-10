#pragma once
#include "Common.h"

//全局只有一个CentralCache，因此可以使用单例饿汉模式
class CentralCache {
public:

	static CentralCache* GetInstance() {
		return &_sInst;
	}

	//从中心缓存获取一个非空的Span
	Span* GetOneSpan(SpanList& list,size_t size);

	//从中心缓存获取一定数量的对象给thread cache 
	size_t FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size);

	//将一定数量的内存块释放到Span中
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanLists[NFREE_LISTS];
private:
	CentralCache(){}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;
};