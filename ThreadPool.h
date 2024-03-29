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

	class Pool;

	class TaskBase
	{
		friend class Pool;

	protected:
		TaskBase();
	public:
		virtual TaskStatus GetTaskStatus() const;
	protected:
		void SetTaskStatus(TaskStatus status);

	private:
		std::atomic<TaskStatus> status;
	};
	template<class RetType>
	class Task :public TaskBase
	{
		friend class Pool;
	public:
		static std::shared_ptr<Task<RetType>> CreateTask(std::future<RetType> res);

	protected:
		Task(std::future<RetType> res);
	public:
		TaskStatus GetTaskStatus() const override;
		RetType GetResult();

	private:
		std::future<RetType> res;
	};
	template<>
	class Task<void> :public TaskBase
	{
		friend class Pool;
	public:
		static std::shared_ptr<Task<void>> CreateTask(std::future<void> res);

	protected:
		Task(std::future<void> res);
	public:
		TaskStatus GetTaskStatus() const override;
		void GetResult();

	private:
		std::future<void> res;
	};

	class Pool
	{
	public:
		Pool (size_t size = 4);
		~Pool();

	public:
		static Pool* GetPool();

		template<class Func, class... Args>
		auto AddTask(Func&& func, Args&&... args) -> std::shared_ptr<Task<decltype(func(args...))>>;

	private:
		static Pool* thisPool;

		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;
		std::mutex queueLock;
		std::condition_variable cv;
		std::atomic<bool> stop;
	};
}

template<class RetType>
inline std::shared_ptr<ThreadPool::Task<RetType>> ThreadPool::Task<RetType>::CreateTask(std::future<RetType> res)
{
	return std::shared_ptr<ThreadPool::Task<RetType>>(new ThreadPool::Task<RetType>(std::move(res)));
}

template<class RetType>
inline ThreadPool::Task<RetType>::Task(std::future<RetType> res) :res(std::move(res)), TaskBase()
{
}

template<class RetType>
inline ThreadPool::TaskStatus ThreadPool::Task<RetType>::GetTaskStatus() const
{
	auto r(this->res.valid());
	if (r)
	{
		return ThreadPool::TaskStatus::FINISHED;
	}
	return TaskBase::GetTaskStatus();
}

template<class RetType>
inline RetType ThreadPool::Task<RetType>::GetResult()
{
	return this->res.get();
}

inline std::shared_ptr<ThreadPool::Task<void>> ThreadPool::Task<void>::CreateTask(std::future<void> res)
{
	return std::shared_ptr<ThreadPool::Task<void>>(new ThreadPool::Task<void>(std::move(res)));
}

inline ThreadPool::Task<void>::Task(std::future<void> res) :res(std::move(res)), TaskBase()
{
}

inline ThreadPool::TaskStatus ThreadPool::Task<void>::GetTaskStatus() const
{
	auto r(this->res.valid());
	if (r)
	{
		return ThreadPool::TaskStatus::FINISHED;
	}
	return TaskBase::GetTaskStatus();
}

inline void ThreadPool::Task<void>::GetResult()
{
	return this->res.get();
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
	auto res(ThreadPool::Task<RetType>::CreateTask(task->get_future()));

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