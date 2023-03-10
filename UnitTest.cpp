#include "ObjectPool.h"
#include "ConcurrentAlloc.h"

void Alloc1() {
	for (int i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2() {
	for (int i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(7);
	}
}
void TLSTest(){

	std::thread t1(Alloc1);
	t1.join();
	std::thread t2(Alloc2);
	t2.join();
}
void ConcurrenetAllocTest1() {
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1<<endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5;
}

void ConcurrenetAllocTest2() {
	for (size_t i = 0; i < 1024; i++) {
		void* p1 = ConcurrentAlloc(6);
		cout << p1<<endl;
	}
	void* p2 = ConcurrentAlloc(6);
	cout << p2;
}
int main() {
	//TestObjectPool();
	//TLSTest();
	//cout << sizeof(PAGE_ID);
	ConcurrenetAllocTest2();

}
