#pragma once
#include "Common.h"
#include "ThreadCache.h"



//申请内存  //通过TLS每个线程无锁地创建thread_cache
static void* ConcurrentAlloc(size_t size) {
	if (tls_threadcache == nullptr) {
		tls_threadcache = new ThreadCache;
	}
	//cout << std::this_thread::get_id() << " : " << tls_threadcache << endl;

	return tls_threadcache->Allocate(size);
}

//释放内存
static void ConcurrentFree(void* ptr, size_t size) {
	assert(tls_threadcache);
	tls_threadcache->Deallocate(ptr, size);
}