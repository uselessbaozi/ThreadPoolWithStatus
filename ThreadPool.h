#pragma once
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

// �̳߳�
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

	protected:
		Task(std::future<RetType> res);

	public:
		TaskStatus GetTaskStatus() const override;
		RetType GetResult();

	private:
		static std::shared_ptr<Task<RetType>> CreateTask(std::future<RetType> res);

	private:
		std::future<RetType> res;
	};
	template<>
	class Task<void> :public TaskBase
	{
		friend class Pool;

	protected:
		Task(std::future<void> res);

	public:
		TaskStatus GetTaskStatus() const override;
		void GetResult();

	private:
		static std::shared_ptr<Task<void>> CreateTask(std::future<void> res);

	private:
		std::future<void> res;
	};

	class Pool
	{
	public:
		Pool (size_t size = 4);
		Pool (const Pool&) = delete;
		Pool& operator=(const Pool&) = delete;
		Pool (Pool&&) = delete;
		Pool& operator=(Pool&&) = delete;
		~Pool();

	public:
		static Pool* GetPool() noexcept;
		template<class Func, class... Args>
		auto AddTask(Func&& func, Args&&... args) -> std::shared_ptr<Task<decltype(func(args...))>>;
		template<class Func, class Instance, class... Args>
		auto AddTask(Func&& func, Instance&& instance, Args&&... args) -> std::shared_ptr<Task<decltype((instance->*func)(args...))>>;
		void WaitAll();

	private:
		static void SetWaitStateFalse() noexcept;
		template<class T, class R>
		void AddTaskToQueue(T& t, R& r);

	private:
		static Pool* thisPool;

		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;
		std::mutex queueLock;
		std::condition_variable cv;
		std::barrier<void(*)()noexcept> waitBarrier;
		bool waitState;
		bool stopState;
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

	AddTaskToQueue(task, res);

	return res;
}

template<class Func, class Instance, class ...Args>
inline auto ThreadPool::Pool::AddTask(Func&& func, Instance&& instance, Args && ...args) -> std::shared_ptr<Task<decltype((instance->*func)(args ...))>>
{
	using RetType = decltype((instance->*func)(args...));

	auto task(
		std::make_shared<std::packaged_task<RetType()>>(
			std::bind(func, instance, std::forward<Args>(args)...)
		)
	);
	auto res(ThreadPool::Task<RetType>::CreateTask(task->get_future()));

	AddTaskToQueue(task, res);

	return res;
}

template<class T, class R>
inline void ThreadPool::Pool::AddTaskToQueue(T& t, R& r)
{
	if (stopState)
	{
		throw std::runtime_error("Pool is stopped.");
	}

	{
		std::lock_guard<std::mutex> lock(queueLock);
		tasks.emplace([t, r]() {
			r->SetTaskStatus(ThreadPool::TaskStatus::RUNNING);
			(*t)();
			r->SetTaskStatus(ThreadPool::TaskStatus::FINISHED);
			});
	}
	cv.notify_one();
}