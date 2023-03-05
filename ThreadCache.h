#pragma once
#include "Common.h"
// ThreadCache是每个线程独有的，用于小于256KB内存的分配。
// 当线程需要小于等于256KB的内存时，会直接从ThreadCache申请。

class ThreadCache {
public:

	//申请内存
	void* Allocate(size_t size);

	//释放内存
	void Deallocate(void* ptr, size_t size);

	//从central cache获取size大小内存对象
	void* FetchFromCentralCache(size_t index, size_t size);

	
private:
	FreeList _freeLists[NFREE_LISTS];
};

// Thread cache local storage
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;