#pragma once
#include <chrono>
#include <memory>
#include <queue>

#include "Job.h"
#include "Lock.h"


namespace CoreThread::Job {
    class JobQueue;
    struct JobData {
        JobData(std::weak_ptr<JobQueue> owner, std::shared_ptr<Job> job) : m_owner(owner), m_job(job) {
        }

        std::weak_ptr<JobQueue> m_owner;
        std::shared_ptr<Job> m_job;
    };

    struct TimerItem {
        bool operator<(const TimerItem& other) const {
            return m_execute_tick > other.m_execute_tick;
        }


        std::chrono::time_point<std::chrono::steady_clock> m_execute_tick;
        JobData* m_job_data = nullptr;
    };

    class JobTimer {
    public:
        void Reserve(std::uint64_t tick_after, std::weak_ptr<JobQueue> owner, std::shared_ptr<Job> job);
        void Distribute(std::chrono::time_point<std::chrono::steady_clock> now);
        void Clear();

    private:
        USE_LOCK;
        CoreMemory::PriorityQueue<TimerItem> m_items;
        std::atomic<bool> m_distributing = false;
    };
}

namespace Global {
    inline std::unique_ptr<CoreThread::Job::JobTimer> GJobTimer = std::make_unique<CoreThread::Job::JobTimer>();
}
