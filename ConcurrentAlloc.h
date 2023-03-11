#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

//申请内存  //通过TLS每个线程无锁地创建thread_cache

/*
	1.若申请的内存<=256kB   则用三层缓存
	2.若申请的内存 >256kB	   则：
			a.  256kB < size <= NPages(128)*8KB   ->page cache
			b.	size > NPages(128)*8KB    ->找系统的堆申请
*/

static void* ConcurrentAlloc(size_t size) {
	
	if (size > MAX_BYTES) {//大于256KB的申请

		size_t alignSize = SizeCLass::RoundUp(size);
		size_t kPages = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPages);
		span->_objectSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;

	}
	else {
		if (tls_threadcache == nullptr) {
			static ObjectPool<ThreadCache>tcPool;
			//tls_threadcache = new ThreadCache;
			tls_threadcache = tcPool.New();
		}
		//cout << std::this_thread::get_id() << " : " << tls_threadcache << endl;

		return tls_threadcache->Allocate(size);
	}
	
}

//释放内存
static void ConcurrentFree(void* ptr) {
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);//获取ptr所在的span
	size_t size = span->_objectSize;

	if (size > MAX_BYTES) {//大于256KB的释放
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else {
		assert(tls_threadcache);
		tls_threadcache->Deallocate(ptr, size);
	}

}