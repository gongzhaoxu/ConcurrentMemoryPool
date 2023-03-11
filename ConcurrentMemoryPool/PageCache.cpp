#include "PageCache.h"

PageCache PageCache::_sInst;

//��CentralCache kҳ��С��Span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	//����>128ҳ�Ĵ��ڴ�����,�������
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_pageID = (PAGE_ID)(ptr) >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap.Set(span->_pageID, span);

		return span;
	}

	//�ȼ��page cache��k��Ͱ��û��span
	if (!_spanLists[k].IsEmpty()) {

		Span* kSpan = _spanLists[k].PopFront();

		//����page_id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
		for (PAGE_ID i = 0; i < kSpan->_n; i++) {
			//_idSpanMap[kSpan->_pageID + i] = kSpan;
			_idSpanMap.Set(kSpan->_pageID + i, kSpan);
		}

		//����kҳ��span
		return kSpan;
	}

	//�ټ��һ�º����Ͱ��û��span������У�����Խ����з�
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_spanLists[i].IsEmpty()) {
			//��ʼ�з�,��iҳ��span�зֳ�kҳ��span��һ��i-kҳ��span
			//kҳ��span���ظ�central cache
			//i-kҳ��span�ҵ���i-k��Ͱ��ȥ
			Span* iSpan = _spanLists[i].PopFront();
			//Span* kSpan = new Span;
			Span* kSpan = _spanPool.New();

			//��iSpan��ͷ����һ��kҳ����
			kSpan->_pageID = iSpan->_pageID;
			kSpan->_n = k;

			iSpan->_pageID += k;
			iSpan->_n -= k;


			//��i-kҳ��span�ҵ���i-k��Ͱ��ȥ
			_spanLists[iSpan->_n].PushFront(iSpan);
			//�洢iSpan����λҳ�ź͸�iSpan��ӳ�䣬����pagecache����	�ڴ�ʱ���кϲ�����
			//_idSpanMap[iSpan->_pageID] = iSpan;
			_idSpanMap.Set(iSpan->_pageID, iSpan);
			//_idSpanMap[iSpan->_pageID + iSpan->_n - 1] = iSpan;
			_idSpanMap.Set(iSpan->_pageID + iSpan->_n - 1, iSpan);


			//����page_id��span��ӳ�䣬����central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; i++) {
				//_idSpanMap[kSpan->_pageID + i] = kSpan;
				_idSpanMap.Set(kSpan->_pageID + i, kSpan);
			}

			//����kҳ��span
			return kSpan;
		}
	}

	//�ߵ���һ����˵�������Ҳ�����ҳ��spanȥ�з���
	//��ʱ��ȥ�Ҷ�Ҫһ��128ҳ��span
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageID = (PAGE_ID)ptr >> 13;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}

//��ȡ�ڴ���Span��ӳ��
Span* PageCache::MapObjectToSpan(void* obj) {
	//ͨ���ڴ��ĵ�ַ��������ҳ�ţ�Ȼ��ͨ��page_id��span��ӳ��Map����֪���ڴ�����ĸ�span
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);

	//std::unique_lock<std::mutex>lock(_pageMtx);
	//auto ret = _idSpanMap.find(id);
	//if (ret != _idSpanMap.end()) {
	//	return ret->second;
	//}
	//else {
	//	assert(false);//�������Ҳ���
	//	return nullptr;
	//}

	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

//��central cache��span�黹��page cache
void PageCache::ReleaseSpanToPageCache(Span* span) {

	if (span->_n > NPAGES - 1) {//˵�����Ҷ�Ҫ�Ĵ��ڴ�
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}


	//��spanǰ���ҳ���Խ��кϲ��������ڴ���Ƭ����
	//��ǰ�ϲ�
	while (true) {
		PAGE_ID prevId = span->_pageID - 1;

		//auto ret = _idSpanMap.find(prevId);
		////1.ǰ��ҳ��û�У����ϲ���
		//if (ret == _idSpanMap.end()) {
		//	break;
		//}

		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr) {
			break;
		}

		//2.ǰ������ҳ��span��ʹ�ã����ϲ���
		Span* prevSpan = ret;
		if (prevSpan->_isUsed == true) {
			break;
		}
		//3.�ϲ�������128ҳ��NPAGES-1����spanû�취������Ҳ���ϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		//4.�ϲ�
		span->_pageID = prevSpan->_pageID;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	//���ϲ�
	while (true) {
		PAGE_ID nextID = span->_pageID + span->_n;

		//auto ret = _idSpanMap.find(nextID);
		////1.����ҳ��û�У����ϲ���
		//if (ret == _idSpanMap.end()) {
		//	break;
		//}
		auto ret = (Span*)_idSpanMap.get(nextID);
		//1.����ҳ��û�У����ϲ���
		if (ret == nullptr) {
			break;
		}

		//2.��������ҳ��span��ʹ�ã����ϲ���
		Span* nextSpan = ret;
		if (nextSpan->_isUsed == true) {
			break;
		}
		//3.�ϲ�������128ҳ��NPAGES-1����spanû�취������Ҳ���ϲ�
		if (nextSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		//4.�ϲ�
		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);

		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	//�ϲ���span������
	_spanLists[span->_n].PushFront(span);
	span->_isUsed = false;
	//_idSpanMap[span->_pageID] = span;
	_idSpanMap.Set(span->_pageID, span);
	//_idSpanMap[span->_pageID + span->_n - 1] = span;
	_idSpanMap.Set(span->_pageID + span->_n - 1, span);
}