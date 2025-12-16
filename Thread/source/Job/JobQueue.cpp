#include <chrono>

#include "Job/JobQueue.h"
#include "ThreadLocals.h"
#include "Job/GlobalQueue.h"
#include "Job/Job.h"

namespace CoreThread::Job {

    void JobQueue::Push(std::shared_ptr<Job> job, bool pushOnly) {
        const std::int32_t prevCount = m_job_count.fetch_add(1);
        m_jobs.Push(job); // WRITE_LOCK

        // The thread that entered the first job is responsible for execution.
        if (prevCount == 0) {
            // Run if no JobQueue is already running
            if (ThreadLocal::CurrentJobQueue == nullptr && pushOnly == false) {
                Execute();
            }
            else {
                // Passes it to GlobalQueue for execution by another free thread.
                Global::GGlobalQueue->Push(shared_from_this());
            }
        }
    }

    // 1) What if you get too busy with work?
    void JobQueue::Execute() {
        ThreadLocal::CurrentJobQueue = this;

        while (true) {
            CoreMemory::Vector<std::shared_ptr<Job>> jobs;
            m_jobs.PopAll(jobs);

            const std::int32_t jobCount = static_cast<std::int32_t>(jobs.size());
            for (std::int32_t i = 0; i < jobCount; i++)
                jobs[i]->Execute();

            // Quit if there are 0 tasks remaining
            if (m_job_count.fetch_sub(jobCount) == jobCount) {
                ThreadLocal::CurrentJobQueue = nullptr;
                return;
            }

            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            if (now >= ThreadLocal::end_tick_count) {
                ThreadLocal::CurrentJobQueue = nullptr;
                // Passes it to GlobalQueue for execution by another free thread.
                Global::GGlobalQueue->Push(shared_from_this());
                break;
            }
        }
    }
}
