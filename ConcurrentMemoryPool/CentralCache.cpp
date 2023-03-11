#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

//�����Ļ����ȡһ���ǿյ�Span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	//�Ȳ鿴��ǰ��spanlist���Ƿ���δ��������span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList) {//ֻҪ��ǰSpan�µ�_freeList��Ϊ��
			return it;
		}
		else {
			it = it->_next;
		}
	}

	//�Ȱ�central cache��Ͱ�����ˣ�����������������ͷ��ڴ������������
	list._mtx.unlock();

	//�ߵ�����˵��û�п���span�ˣ�ֻ����page cacheҪ�ڴ�,Ҫһ��span
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeCLass::NumMovePage(size));
	span->_isUsed = true;
	span->_objectSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();
	//��NewSpan()������������page cache�����󣬲���Ҫ��central cache��Ͱ��
	//��Ϊ�����߳��ò������span
	

	//span�Ĵ���ڴ���ʼ��ַ��������ַ
	char* startAddress = (char*)(span->_pageID << PAGE_SHIFT);
	
	//span�Ĵ���ڴ��ֽ���
	size_t bytes = span->_n << PAGE_SHIFT;
	char* endAddress = startAddress + bytes;

	//�Ѵ���ڴ��г�����������������
	//1.����һ��������ͷ������β��
	span->_freeList = startAddress;
	startAddress += size;
	//2.β��
	void* tail = span->_freeList;
	
	while (startAddress < endAddress) {
		NextObj(tail) = startAddress;
		tail = NextObj(tail);
		startAddress += size;
	}
	NextObj(tail) = nullptr;

	//�����ϵ�
	//������ѭ���� ��-> ����->ȫ���ж�,����������еĵط�ͣ�¡�
	/*int j = 0;
	void* cur = span->_freeList;
	while (cur) {
		cur = NextObj(cur);
		j++;
	}

	if (j != bytes/size) {
		int debug = 999;
	}*/

	//�к�span�Ժ���Ҫ��span�ҵ�Ͱ����ȥ��ʱ���ټ���
	list._mtx.lock();
	list.PushFront(span);
	return span;
}


//�����Ļ����ȡһ�������Ķ����thread cache
// central cache�� thread cacheӳ���ϵһ�¡�
//batchNumΪ����ĸ�����sizeΪ�����ڴ��С
//��central cache��span����batchNum���ڴ�飬��ָ�batchNum����û����ô���� ���� ��һ����
size_t CentralCache::FetchRangObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeCLass::Index(size);
	_spanLists[index]._mtx.lock();//����Ͱ��

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = start;

	//��span�л�ȡbatchNum������
	//������batchNum�����ж����ö���

	size_t i = 0;
	size_t actualNum = 1;//ע��actualNum��ʼֵΪ1������0
	while (i < batchNum - 1 && NextObj(end) != nullptr) {
		end = NextObj(end);
		i++;
		actualNum++;
	}

	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	//�����ϵ�

	/*int j = 0;
	void* cur = start;
	while (cur) {
		cur = NextObj(cur);
		j++;
	}

	if (j != actualNum) {
		int debug = 999;
	}*/


	_spanLists[index]._mtx.unlock();//��Ͱ��

	return actualNum;
}


//��һ���������ڴ���ͷŵ�Span��
void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	size_t index = SizeCLass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start) {
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//ͷ��
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		//˵��span�зֳ�ȥ������С���ڴ涼�黹������
		//��ô���span�Ϳ��Թ黹��page cache��page cache���Գ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0) {
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			
			//�黹�ϼ�

			_spanLists[index]._mtx.unlock();//���Ͱ��
			PageCache::GetInstance()->_pageMtx.lock();//pagecache����ȫ����
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();//pagecache����ȫ����

			_spanLists[index]._mtx.lock();//��������Ͱ��
		}
		start = next;
	}

	_spanLists[index]._mtx.unlock();
}