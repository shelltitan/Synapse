#include "ThreadManager.hpp"
#include "ThreadLocals.hpp"
#include "Job/GlobalQueue.hpp"
#include "Job/JobQueue.hpp"
#include "Job/JobTimer.hpp"


namespace CoreThread {
    ThreadManager::ThreadManager() {
        // Main Thread
        InitialiseTLS();
    }

    ThreadManager::~ThreadManager() {
        Join();
    }

    auto ThreadManager::Launch(const std::function<void(void)> &callback) -> void {
        std::scoped_lock s_lock{ m_lock };

        (void)m_threads.emplace_back([=]() {
            InitialiseTLS();
            callback();
            DestroyTLS();
        });
    }

    auto ThreadManager::Join() -> void {
        for (std::thread& t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        m_threads.clear();
    }

    auto ThreadManager::InitialiseTLS() -> void {
        static std::atomic<std::uint32_t> s_thread_id = 1;
        ThreadLocal::thread_id = s_thread_id.fetch_add(1);
    }

    auto ThreadManager::DestroyTLS() -> void {
    }

    auto ThreadManager::DoGlobalQueueWork() -> void {
        while (true) {
            std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            if (now > ThreadLocal::end_tick_count) {
                break;
            }

            std::shared_ptr<Job::JobQueue> job_queue = Global::GGlobalQueue->Pop();
            if (job_queue == nullptr) {
                break;
            }

            job_queue->Execute();
        }
    }

    auto ThreadManager::DistributeReservedJobs() -> void {
        const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

        Global::GJobTimer->Distribute(now);
    }
}