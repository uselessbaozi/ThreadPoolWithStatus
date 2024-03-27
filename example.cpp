#include "ThreadPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
using namespace std;

int main()
{
	ThreadPool::Pool pool(16);
	vector<shared_ptr<ThreadPool::Task<int>>> results;

	for (int i = 0; i < 800; ++i)
	{
		results.push_back(pool.AddTask([i]() {cout << "Count: " << i << endl; return i; }));
	}
	//this_thread::sleep_for(chrono::seconds(5));
	for (auto& r : results)
	{
		cout << r->GetTaskStatus() << endl;
		cout << r->GetResult() << endl;
	}
	std::cout << std::endl;
	return 0;
}