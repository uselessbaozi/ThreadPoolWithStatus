#include "ThreadPool.h"

ThreadPool::TaskBase::TaskBase() :status(ThreadPool::TaskStatus::READY)
{
}

ThreadPool::TaskStatus ThreadPool::TaskBase::GetTaskStatus() const
{
	return status.load();
}

void ThreadPool::TaskBase::SetTaskStatus(TaskStatus status)
{
	this->status.store(status);
}

ThreadPool::Pool::Pool(size_t size): stop(false)
{
	if (ThreadPool::Pool::GetPool())
		throw std::runtime_error("ThreadPool::Pool::GetPool() must be called before creating a task");
	thisPool = this;
	for (auto i = 0u; i < size; ++i)
	{
		workers.emplace_back([this] {
			while (1)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(queueLock);
					cv.wait(lock, [this] { return this->stop.load() || !this->tasks.empty(); });
					if(this->stop.load() && this->tasks.empty())
						return;
					task = std::move(this->tasks.front());
					this->tasks.pop();
				}
				task();
			}
		});
	}
}

ThreadPool::Pool::~Pool()
{
	stop.store(true);
	cv.notify_all();
	for (auto& worker : workers)
		worker.join();
}

ThreadPool::Pool* ThreadPool::Pool::thisPool = nullptr;

ThreadPool::Pool* ThreadPool::Pool::GetPool()
{
	return thisPool;
}
