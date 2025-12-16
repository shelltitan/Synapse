#include "Job/JobTimer.h"
#include "Job/JobQueue.h"

namespace CoreThread::Job {
    void JobTimer::Reserve(std::uint64_t tick_after, std::weak_ptr<JobQueue> owner, std::shared_ptr<Job> job) {
        const std::chrono::time_point<std::chrono::steady_clock> execute_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(tick_after);
        JobData* jobData = CoreMemory::ObjectPool<JobData>::Pop(owner, job);

        WRITE_LOCK;

        m_items.push(TimerItem{ execute_tick, jobData });
    }

    void JobTimer::Distribute(std::chrono::time_point<std::chrono::steady_clock> now) {
        // Only 1 thread passes at a time
        if (m_distributing.exchange(true) == true) {
            return;
        }

        CoreMemory::Vector<TimerItem> items;

        {
            WRITE_LOCK;

            while (m_items.empty() == false) {
                const TimerItem& timerItem = m_items.top();
                if (now < timerItem.m_execute_tick) {
                    break;
                }

                items.push_back(timerItem);
                m_items.pop();
            }
        }

        for (TimerItem& item : items) {
            if (std::shared_ptr<JobQueue> owner = item.m_job_data->m_owner.lock())
                owner->Push(item.m_job_data->m_job);

            CoreMemory::ObjectPool<JobData>::Push(item.m_job_data);
        }

        // Release when finished.
        m_distributing.store(false);
    }

    void JobTimer::Clear() {
        WRITE_LOCK

        while (m_items.empty() == false) {
            const TimerItem& timerItem = m_items.top();
            CoreMemory::ObjectPool<JobData>::Push(timerItem.m_job_data);
            m_items.pop();
        }
    }
}