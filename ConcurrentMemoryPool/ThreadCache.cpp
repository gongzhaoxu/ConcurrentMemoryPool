#include "ThreadCache.h"
#include "CentralCache.h"

//windows.h中有min max宏定义，会将algorithm中的min max函数模版当做宏来处理，需要在此解除宏
#undef max
#undef min

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

	//检查当链表长度大于一次批量申请的内存长度时就还一段list给central cache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
		ListTooLong(_freeLists[index], size);
	}


}

//当申请的内存大于256KB时，像central cache申请,然后将申请的内存块插入到_freeLists[index]中
//并且返回头一个内存块。
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//采取慢开始反馈策略申请
	// 1. 最开始不会向central cache要太多，因为太多了可能用不完浪费
	// 2. 若不断有需求，则batchNum不断增长，直到上限NumMoveSize;
	// 3. size越大，一次向central cache要的batchNum越大
	// 5. size越小，一次向central cache要的batchNum越小
	size_t batchNum = std::min( _freeLists[index].MaxSize(), SizeCLass::NumMoveSize(size) );

	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::GetInstance()->FetchRangObj(start, end, batchNum, size);
	assert(actualNum > 0);//至少要给thread cache一个
	if (actualNum == 1) {
		assert(start == end);
	}
	else {
		//[start,end]是thread cache向central cache申请的内存块范围
		//我们需要将[start,end]插入到_freeLists[index]中，并且返回第一个内存块
		//下面那行代码等价于
			/*	_freeLists[index].PushRange(start, end);
				return _freeLists[index].Pop();			*/

		_freeLists[index].PushRange(NextObj(start), end,actualNum-1);
	}
	return start;
}

//释放对象时，链表过长时，回收内存回到中心缓存
void ThreadCache::ListTooLong(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}