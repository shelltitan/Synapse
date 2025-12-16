#pragma once

#include <functional>
#include <mutex>
#include <thread>

namespace CoreThread {
    class ThreadManager {
    public:
        ThreadManager();
        ~ThreadManager();

        auto Launch(const std::function<void()> &callback) -> void;
        auto Join() -> void;

        static auto InitialiseTLS() -> void;
        static auto DestroyTLS() -> void;

        static auto DoGlobalQueueWork() -> void;
        static auto DistributeReservedJobs() -> void;

    private:
        std::mutex m_lock;
        std::vector<std::thread> m_threads;
    };
}

namespace Global {
    inline std::unique_ptr<CoreThread::ThreadManager> GThreadManager = std::make_unique<CoreThread::ThreadManager>();
}