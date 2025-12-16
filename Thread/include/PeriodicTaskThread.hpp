#pragma once

#include <functional>
#include <map>
#include <mutex>

#include "Thread.h"

namespace CoreThread {
    class PeriodicTaskThread : public Thread<PeriodicTaskThread> {
        struct PeriodicTask {
            int time_period;
            std::chrono::time_point<std::chrono::steady_clock> last_time;
            std::function<void(std::uint32_t, std::chrono::time_point<std::chrono::steady_clock>)> m_func;
            PeriodicTask() : time_period(), last_time() {};
        };

    public:
        PeriodicTaskThread();
        ~PeriodicTaskThread();

        bool Initialise();
        void Release();

        template <typename T>
        bool RegisterPeriodicTask(const std::uint32_t id, const std::uint32_t time_period, T* fnClass, void (T::*fn)(std::uint32_t, std::chrono::time_point<std::chrono::steady_clock>)) {
            std::scoped_lock lock{ m_sync };
            std::map<std::uint32_t, PeriodicTask>::iterator it = m_elements.find(id);
            if (it != m_elements.end()) {
                return false;
            }

            PeriodicTaskThread::PeriodicTask element;
            element.m_func = std::bind(fn, fnClass, std::placeholders::_1, std::placeholders::_2);
            element.time_period = time_period;
            element.last_time = m_current_time;

            m_elements[id] = element;

            return true;
        }

        void KillPeriodicTask(const std::uint32_t id);

        void RunThreadProcess();
        void ImplementCloseThread();

    private:
        std::mutex m_sync;
        std::map<std::uint32_t, PeriodicTask> m_elements;
        std::chrono::time_point<std::chrono::steady_clock> m_current_time;
    };
}