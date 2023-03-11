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
	void* p6 = ConcurrentAlloc(8);
	void* p7 = ConcurrentAlloc(8);

	cout << p1<<endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
	cout << p6 << endl;
	cout << p7 << endl;

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
	ConcurrentFree(p6);
	ConcurrentFree(p7);

}

void ConcurrenetAllocTest2() {
	for (size_t i = 0; i < 1024; i++) {
		void* p1 = ConcurrentAlloc(6);
		cout << p1<<endl;
	}
	void* p2 = ConcurrentAlloc(6);
	cout << p2;
}
void TestMultiThread1() {
	std::vector<void*>v;
	for (int i = 0; i < 7; i++) {
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}
	for (auto e : v) {
		ConcurrentFree(e);
	}
}

void TestMultiThread2() {
	std::vector<void*>v;
	for (int i = 0; i < 7; i++) {
		void* ptr = ConcurrentAlloc(7);
		v.push_back(ptr);
	}
	for (auto e : v) {
		ConcurrentFree(e);
	}
}
void TestMultiThread() {

	std::thread t1(TestMultiThread1);
	std::thread t2(TestMultiThread2);
	t1.join();
	t2.join();
}

void BigAlloc() {
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);
	
	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}

struct TreeNode {
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool() {

	//测试的轮次
	const size_t Rounds = 3;
	//每次申请释放次数
	const size_t N = 100000;

	//测试new delete效率
	std::vector<TreeNode*>v1;
	v1.reserve(N);
	size_t begin1 = clock();
	for (int i = 0; i < Rounds; i++) {
		for (int j = 0; j < N; j++) {
			v1.push_back(new TreeNode);
		}
		for (int j = 0; j < N; j++) {
			delete v1[j];
		}
		v1.clear();
	}
	size_t end1 = clock();

	//测试定长内存池效率
	std::vector<TreeNode*>v2;
	v2.reserve(N);
	ObjectPool<TreeNode>TNPool;
	size_t begin2 = clock();
	for (int i = 0; i < Rounds; i++) {
		for (int j = 0; j < N; j++) {
			v2.push_back(TNPool.New());
		}
		for (int j = 0; j < N; j++) {
			TNPool.Delete(v2[j]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new cost time = " << end1 - begin1 << endl;
	cout << "object pool cost time = " << end2 - begin2 << endl;
}

//int main() {
//	//TestObjectPool();
//	//TLSTest();
//	//cout << sizeof(PAGE_ID);
//	ConcurrenetAllocTest1();
//	//TestMultiThread();
//	BigAlloc();
//
//}
