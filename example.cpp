#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
using namespace std;


void ExampleWithNoReturn()
{
	vector<shared_ptr<ThreadPool::Task<void>>> voidResults;
	for (int i = 0; i < 800; ++i)
	{
		voidResults.push_back(std::move(ThreadPool::Pool::GetPool()->AddTask([i]() {cout << "Count: " << i << endl; })));
	}
	ThreadPool::Pool::GetPool()->WaitAll();
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : voidResults)
	{
		cout << r->GetTaskStatus() << endl;
		//r->GetResult();
	}
	cout << endl;
}

void ExampleWithReturn()
{
	vector<shared_ptr<ThreadPool::Task<int>>> intResults;
	for (int i = 0; i < 800; ++i)
	{
		intResults.push_back(std::move(ThreadPool::Pool::GetPool()->AddTask([i]() {cout << "Count: " << i << endl; return i; })));
	}
	ThreadPool::Pool::GetPool()->WaitAll();
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : intResults)
	{
		cout << r->GetTaskStatus() << endl;
		cout << r->GetResult() << endl;
	}
	cout << endl;
}

void ExampleWithClass()
{
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

	vector<shared_ptr<ThreadPool::Task<void>>> voidResults;
	vector<test> tests;
	for (int i = 0; i < 800; ++i)
	{
		tests.push_back(test(i));
	}
	for (int i = 0; i < 800; ++i)
	{
		voidResults.push_back(std::move(ThreadPool::Pool::GetPool()->AddTask(&test::ttt, &tests[i])));
	}
	ThreadPool::Pool::GetPool()->WaitAll();
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : voidResults)
	{
		cout << r->GetTaskStatus() << endl;
		//r->GetResult();
	}
	cout << endl;
}

int main()
{
	ThreadPool::Pool pool(16);

	ExampleWithNoReturn();
	ExampleWithReturn();
	ExampleWithClass();

	return 0;
}