#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>
#include <memory>

// Ïß³Ì³Ø
namespace ThreadPool
{
	enum TaskStatus
	{
		READY,
		RUNNING,
		FINISHED
	};

	class TaskBase
	{
	public:
		TaskBase();
		TaskStatus GetTaskStatus() const;
		void SetTaskStatus(TaskStatus status);

	private:
		std::atomic<TaskStatus> status;
	};
	template<class RetType>
	class Task :public TaskBase
	{
	public:
		Task(std::future<RetType> res);

	public:
		RetType GetResult();

	private:
		std::future<RetType> res;
	};
	template<>
	class Task<void> :public TaskBase
	{
	public:
		Task(std::future<void> res);

	public:
		void GetResult();

	private:
		std::future<void> res;
	};

	class Pool
	{
	public:
		Pool (size_t size = 4);
		~Pool();

		template<class Func, class... Args>
		auto AddTask(Func&& func, Args&&... args) -> std::shared_ptr<Task<decltype(func(args...))>>;

	private:
		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;
		std::mutex queueLock;
		std::condition_variable cv;
		std::atomic<bool> stop;
	};
}

template<class RetType>
inline ThreadPool::Task<RetType>::Task(std::future<RetType> res) :res(std::move(res)), TaskBase()
{
}

template<class RetType>
inline RetType ThreadPool::Task<RetType>::GetResult()
{
	switch (GetTaskStatus())
	{
	case ThreadPool::TaskStatus::READY:
		throw std::runtime_error("Task is not running yet.");
		break;
	case ThreadPool::TaskStatus::RUNNING:
		throw std::runtime_error("Task is running.");
		break;
	case ThreadPool::TaskStatus::FINISHED:
	default:
		return this->res.get();
	}
}

inline ThreadPool::Task<void>::Task(std::future<void> res) :res(std::move(res)), TaskBase()
{
}

inline void ThreadPool::Task<void>::GetResult()
{
	switch (GetTaskStatus())
	{
	case ThreadPool::TaskStatus::READY:
		throw std::runtime_error("Task is not running yet.");
		break;
	case ThreadPool::TaskStatus::RUNNING:
		throw std::runtime_error("Task is running.");
		break;
	case ThreadPool::TaskStatus::FINISHED:
	default:
		return;
	}
}

template<class Func, class ...Args>
inline auto ThreadPool::Pool::AddTask(Func&& func, Args&& ...args) -> std::shared_ptr<ThreadPool::Task<decltype(func(args...))>>
{
	using RetType = decltype(func(args...));

	auto task(
		std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		)
	);
	auto res(std::make_shared<ThreadPool::Task<RetType>>(task->get_future()));

	if (stop.load())
	{
		throw std::runtime_error("Pool is stopped.");
	}

	{
		std::lock_guard<std::mutex> lock(queueLock);
		tasks.emplace([task, res]() {
			res->SetTaskStatus(ThreadPool::TaskStatus::RUNNING);
			(*task)();
			res->SetTaskStatus(ThreadPool::TaskStatus::FINISHED);
			});
	}
	cv.notify_one();

	return res;
}