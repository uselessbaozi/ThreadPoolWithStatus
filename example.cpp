#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
using namespace std;

class test
{
public:
	test(int i) : m_i(i) {}
	void ttt() const
	{
		cout << "Count: " << m_i << endl;
		//return m_i;
	}
private:
	int m_i;
};

int main()
{
	ThreadPool::Pool pool(16);

	vector<shared_ptr<ThreadPool::Task<void>>> voidResults;
	for (int i = 0; i < 800; ++i)
	{
		voidResults.push_back(std::move(pool.AddTask([i]() {cout << "Count: " << i << endl; })));
	}
	pool.WaitAll();
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : voidResults)
	{
		cout << r->GetTaskStatus() << endl;
		//r->GetResult();
	}
	std::cout << std::endl;
	
	vector<shared_ptr<ThreadPool::Task<int>>> intResults;
	for (int i = 0; i < 800; ++i)
	{
		intResults.push_back(std::move(pool.AddTask([i]() {cout << "Count: " << i << endl; return i; })));
	}
	pool.WaitAll();
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : intResults)
	{
		cout << r->GetTaskStatus() << endl;
		cout << r->GetResult() << endl;
	}
	std::cout << std::endl;

	auto t = test(10);
	auto result = pool.AddTask(&test::ttt, &t);
	pool.WaitAll();

	return 0;
}