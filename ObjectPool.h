#pragma once
#include "Common.h"

//定长内存池
template <class T>
class ObjectPool
{
public:
	T* New() {
		T* obj = nullptr;
		//优先分配自由链表里面还回来的内存块
		if (_freeList) {//头删
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else {
			//如果剩余内存小于一个对象，则重新开辟大块内存
			if (_remainBytes < sizeof(T)) {
				_remainBytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);//128KB
				if (_memory == nullptr) {//申请失败则抛出异常
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//若在32位机下，指针为4B，若T长度<4B，则无法存下指针，链表无法继续下去
			//若在64位机下，指针为8B，若T长度<8B，则无法存下指针，链表无法继续下去
			//因此需要下面的判断
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}
		//定位new，既然已经为obj分配好了内存，那么就可以调用T的构造函数对其显式初始化
		new(obj) T;
		return obj;
	}

	void Delete(T* obj) {
		//显式调用T的析构函数
		obj -> ~T();
		//头插
		*((void**)obj) = _freeList;//用二级指针可以适用于32或64位机
		_freeList = obj;
	}

private:

	char* _memory = nullptr;//指向内存池
	size_t _remainBytes = 0;//内存池在切分过程中剩余字节数
	void* _freeList = nullptr;//还回内存过程中的自由链表的头指针

};
