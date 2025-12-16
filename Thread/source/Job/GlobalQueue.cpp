#include "Job/GlobalQueue.h"
#include "Job/JobQueue.h"

namespace CoreThread::Job {

    GlobalQueue::GlobalQueue() {
    }

    GlobalQueue::~GlobalQueue() {
    }

    void GlobalQueue::Push(std::shared_ptr<JobQueue> jobQueue) {
        m_job_queues.Push(jobQueue);
    }

    std::shared_ptr<JobQueue> GlobalQueue::Pop() {
        return m_job_queues.Pop();
    }
}