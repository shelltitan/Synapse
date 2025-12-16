#pragma once
#include <RedoObjectPool.h>
#include <memory>
#include "JobTimer.h"
#include "LockQueue.h"

namespace CoreThread::Job {
    class Job;
    class JobQueue : public std::enable_shared_from_this<JobQueue> {
    public:
        void DoAsync(std::function<void()>&& callback) {
            Push(CoreMemory::ObjectPool<Job>::MakeShared(std::move(callback)));
        }

        template <typename T, typename Ret, typename... Args>
        void DoAsync(Ret (T::*memFunc)(Args...), Args... args) {
            std::shared_ptr<T> owner = std::static_pointer_cast<T>(shared_from_this());
            Push(CoreMemory::ObjectPool<Job>::MakeShared(owner, memFunc, std::forward<Args>(args)...));
        }

        void DoTimer(std::uint64_t tick_after, std::function<void()>&& callback) {
            std::shared_ptr<Job> job = CoreMemory::ObjectPool<Job>::MakeShared(std::move(callback));
            Global::GJobTimer->Reserve(tick_after, shared_from_this(), job);
        }

        template <typename T, typename Ret, typename... Args>
        void DoTimer(std::uint64_t tick_after, Ret (T::*memFunc)(Args...), Args... args) {
            std::shared_ptr<T> owner = std::static_pointer_cast<T>(shared_from_this());
            std::shared_ptr<Job> job = CoreMemory::ObjectPool<Job>::MakeShared(owner, memFunc, std::forward<Args>(args)...);
            Global::GJobTimer->Reserve(tick_after, shared_from_this(), job);
        }

        void ClearJobs() { m_jobs.Clear(); }

    public:
        void Push(std::shared_ptr<Job> job, bool push_only = false);
        void Execute();

    protected:
        LockQueue<std::shared_ptr<Job>> m_jobs;
        std::atomic<std::int32_t> m_job_count = 0;
    };
}
