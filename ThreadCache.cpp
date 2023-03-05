#include "ThreadCache.h"
#include "CentralCache.h"
void* ThreadCache::Allocate(size_t size) {

	assert(size <= MAX_BYTES);
	size_t allocSize = SizeCLass::RoundUp(size);//计算对齐后实际分配的内存大小  allocSize>=size
	size_t index = SizeCLass::Index(size);//计算映射到哪个桶


	//如果对应的自由链表桶不为空，直接从桶中取出内存块
	//如果为空，则从central_cache中获取内存块
	if (!_freeLists[index].IsEmpty()) {
		return _freeLists[index].Pop();
	}
	else {
		return FetchFromCentralCache(index, allocSize);
	}
}
void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(size <= MAX_BYTES);
	assert(ptr);
	size_t index = SizeCLass::Index(size);//计算映射到哪个桶
	_freeLists[index].Push(ptr);//头插

}

//当申请的内存大于256KB时，像central cache申请
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//采取慢开始反馈策略申请
	// 1. 最开始不会向central cache要太多，因为太多了可能用不完浪费
	// 2. 若不断有需求，则batchNum不断增长，直到上限NumMoveSize;
	// 3. size越大，一次向central cache要的batchNum越大
	// 5. size越小，一次向central cache要的batchNum越小
	size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeCLass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::GetInstance()->FetchRangObj(start, end, batchNum, size);
	assert(actualNum > 1);//至少要给thread cache一个



	

	return nullptr;
}
