#pragma once
#include <memory>
#include "LockQueue.h"

namespace CoreThread::Job {
    class JobQueue;
    class GlobalQueue {
    public:
        GlobalQueue();
        ~GlobalQueue();

        void Push(std::shared_ptr<JobQueue> job_queue);
        std::shared_ptr<JobQueue> Pop();

    private:
        LockQueue<std::shared_ptr<JobQueue>> m_job_queues;
    };
}

namespace Global {
    inline std::unique_ptr<CoreThread::Job::GlobalQueue> GGlobalQueue = std::make_unique<CoreThread::Job::GlobalQueue>();
}
