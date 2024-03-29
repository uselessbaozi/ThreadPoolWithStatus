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

ThreadPool::Pool::Pool(size_t size) : stopState(false), waitState(false), waitBarrier(size + 1, ThreadPool::Pool::SetWaitStateFalse)
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
					cv.wait(lock, [this] { return this->stopState || !this->tasks.empty() || this->waitState; });
					if (this->stopState && this->tasks.empty())
						return;
					else if (this->waitState && this->tasks.empty())
					{
						lock.unlock();
						waitBarrier.arrive_and_wait();
						continue;
					}
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
	stopState = true;
	cv.notify_all();
	for (auto& worker : workers)
		worker.join();
}

ThreadPool::Pool* ThreadPool::Pool::thisPool = nullptr;

ThreadPool::Pool* ThreadPool::Pool::GetPool() noexcept
{
	return thisPool;
}

void ThreadPool::Pool::WaitAll()
{
	waitState = true;
	cv.notify_all();
	waitBarrier.arrive_and_wait();
}

void ThreadPool::Pool::SetWaitStateFalse() noexcept
{
	thisPool->waitState = false;
}