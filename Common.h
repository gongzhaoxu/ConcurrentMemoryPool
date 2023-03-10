#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <assert.h>
#include <time.h>
#include <windows.h>
using std::cout;
using std::endl;


//定义一个MAX_BYTES,如果申请的内存块小于MAX_BYTES,就从thread_cache申请
//大于MAX_BYTES, 就直接从page_cache中申请
static const size_t MAX_BYTES = 256 * 1024;


//thread_cache和central cache中自由链表哈希桶的表大小
static const size_t NFREE_LISTS = 208;

//Page cache中哈希桶的表大小
static const size_t NPAGES = 129;

//页大小转换偏移量，一页为2^13B，也就是8KB    将大小右移13位可得到需要多少页
static const size_t PAGE_SHIFT = 13;

//条件编译
//32位环境下，_WIN32有定义，_WIN64无定义
//64位环境下，_WIN32和_WIN64都有定义
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif // _WIN64


//直接去堆上申请k页的空间  
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//在linux下申请 
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

//获取内存对象中存储的头4bit或者8bit值，即链接下一个对象的地址的引用 *&
static inline void*& NextObj(void* obj) {
	return *(void**)obj;
}

//管理切分好的小内存对象的自由链表
class FreeList {
public:

	void Push(void* obj) {
		//头插
		//*(void**)obj = _freeList;
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	void PushRange(void* start, void* end,size_t n) {
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	void PopRange(void*& start, void*& end, size_t n) {
		assert(n >= _size);
		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; i++) {
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	void* Pop() {
		//头删
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;

		return obj;
	}

	//判断_freeList是否为空
	bool IsEmpty() {
		return _freeList == nullptr;
	}

	size_t& MaxSize() {
		return _maxSize;
	}

	size_t Size() {
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size;
};


/********************************************************
 * 自由链表的哈希桶位置和申请内存块大小的映射关系				 *
 * ThreadCache是哈希桶结构，每个桶对应了一个自由链表。			 *
 * 每个自由链表中，分配的块的大小是固定的。						 *
 * 每个桶是按照自由链表中内存块的大小来对应映射位置的。			 *
 * 如果从1B到256KB每个字节都分配一个哈希桶，这并不现实。			 *
 * 因此，可以采用对齐数的方式。即某个范围的内存块大小映射到某个桶	 *
 ********************************************************/

 //计算对象大小的映射规则
class SizeCLass {
public:
	// 整体控制在最多11%左右的内碎片浪费            共计208个桶
	// [1,128]                  8byte对齐        [0,16)号哈希桶    16个桶
	// [128+1,1024]             16byte对齐       [16,72)号哈希桶	  56个桶
	// [1024+1,8*1024]          128byte对齐      [72,128)号哈希桶  56个桶
	// [8*1024+1,64*1024]       1024byte对齐     [128,184)号哈希桶 56个桶
	// [64*1024+1,256*1024]     8*1024byte对齐   [184,208)号哈希桶 24个桶


	//给定需要的字节数和对齐数，返回实际申请的内存块大小,采取向上对齐方法
	static inline size_t _RoundUp(size_t bytes, size_t alignNum) {
		return (bytes + alignNum - 1) & ~(alignNum - 1);
	}
	static inline size_t RoundUp(size_t size) {
		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024) {
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size, 8 * 1024);
		}
		else {
			assert(false);
			return -1;
		}
	}
	//align_shift是对齐数的偏移量，如对齐数是8KB，偏移量就是13
	static inline size_t _Index(size_t bytes, size_t align_shift) {
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//给定所申请的内存块大小，寻找在哪个自由链表桶
	static inline size_t Index(size_t bytes) {
		assert(bytes <= MAX_BYTES);
		//用一个数组存储，每个区间有多少个桶
		static int group[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);  //8B对齐  16个桶
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group[0];  //16B对齐  56个桶
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group[0] + group[1];  //128B对齐  56个桶
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group[0] + group[1] + group[2];  //1024B对齐  56个桶
		}
		else if (bytes <= 256 * 1024) {		//8*1024B对齐  24个桶
			return _Index(bytes - 64 * 1024, 13) + group[0] + group[1] + group[2] + group[3];  //8*1024B对齐
		}
		else {
			assert(false);
			return -1;
		}
	}

	//thread cache一次从central cache获取多少个  函数参数size指单个小内存块大小
	static size_t NumMoveSize(size_t size) {
		assert(size > 0);

		//num介于2到512之间
		//大对象一次获得的少，小对象一次获得的多,避免内存的浪费
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;

		return num;
	}

	//central cache一次从page cache获取多少页  函数参数size指单个小内存块大小
	//单个对象 8B...
	// ...
	//单个对象 256KB...
	static size_t NumMovePage(size_t size) {
		assert(size > 0);
		size_t num = NumMoveSize(size);
		size_t bytes = num * size;//一共需要多少byte

		int pageNum = bytes >> PAGE_SHIFT;

		if (pageNum == 0) {//不满一页则调一页
			pageNum = 1;
		}
		return pageNum;
	}

private:

};

// PageCache和CentralCache的桶挂的都是以页为单位Span链表,
// Span链表是以带头双向循环链表的形式存在的
// 管理多个连续页大块内存跨度的结构
class Span {
public:

	size_t _pageID = 0;//大块内存起始页的页号
	size_t _n = 0;//页的数量

	Span* _next = nullptr;//Span双向链表的的结构
	Span* _prev = nullptr;

	size_t _useCount = 0;//切好小块内存，被分配给thread cache的计数
	void* _freeList = nullptr;//切好的小块内存的自由链表
};

//带头双向循环链表，带头结点，方便O(1)增删
class SpanList {
public:
	SpanList() {//构造函数
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}


	Span* Begin() {
		return _head->_next;
	}
	Span* End() {
		return _head;
	}

	bool IsEmpty() {
		return _head == _head->_next;
	}


	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);
		Span* prev = pos->_prev;
		//prev ->  newSpan ->  pos
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos) { //erase并没有delete掉pos,只是让其从链中脱离
		assert(pos);
		assert(pos != _head);//不能删头结点

		Span* prev = pos->_prev;
		prev->_next = pos->_next;
		pos->_next->_prev = prev;

	}

	//头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	//返回头
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}


private:
	Span* _head;
public:
	std::mutex _mtx;//桶锁
};
