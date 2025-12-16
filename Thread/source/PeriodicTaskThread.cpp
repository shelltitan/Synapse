#include "PeriodicTaskThread.h"

namespace CoreThread {
    PeriodicTaskThread::PeriodicTaskThread() : m_current_time() {
    }

    PeriodicTaskThread::~PeriodicTaskThread() {
        {
            std::scoped_lock sync_lock{ m_sync };
            m_elements.clear();
        }
    }

    bool PeriodicTaskThread::Initialise() {
        return (StartThread());
    }

    void PeriodicTaskThread::Release() {
        CloseThread();
    }

    void PeriodicTaskThread::KillPeriodicTask(const std::uint32_t id) {
        std::scoped_lock sync_lock{ m_sync };
        std::map<std::uint32_t, PeriodicTask>::iterator it = m_elements.find(id);
        if (it != m_elements.end()) {
            m_elements.erase(it);
        }
    }

    void PeriodicTaskThread::RunThreadProcess() {
        while (m_thread_running) {
            std::chrono::time_point<std::chrono::steady_clock> current_time = std::chrono::steady_clock::now();

            {
                std::scoped_lock sync_lock{ m_sync };
                for (std::pair<const std::uint32_t, PeriodicTask>& elem : m_elements) {
                    if (current_time > elem.second.last_time + std::chrono::milliseconds(elem.second.time_period)) {
                        elem.second.last_time = current_time;
                        elem.second.m_func(elem.first, current_time);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void PeriodicTaskThread::ImplementCloseThread() {
        if (m_thread_running) {
            StopRunning();
        }

        if (m_thread != nullptr) {
            m_thread->join();
            delete m_thread;
            m_thread = nullptr;
        }
    }
}