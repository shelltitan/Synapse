#pragma once

#include <thread>

namespace CoreThread {
    template <class Derived>
    class Thread {
    public:
        Thread() noexcept;
        ~Thread() noexcept;

        auto StartThread() noexcept -> bool;
        auto StopRunning() noexcept -> void;
        auto CloseThread() noexcept -> void;

    protected:
        std::thread* m_thread{ nullptr };
        bool m_thread_running;
    };

    template <class Derived>
    Thread<Derived>::Thread() noexcept {
        m_thread_running = false;
    }

    template <class Derived>
    Thread<Derived>::~Thread() noexcept {
        CloseThread();
    }

    template <class Derived>
    auto Thread<Derived>::StartThread() noexcept -> bool {
        if (m_thread_running) {
            return false;
        }
        m_thread = new std::thread(&Derived::RunThreadProcess, static_cast<Derived*>(this));
        if (m_thread == nullptr) {
            return false;
        }
        m_thread_running = true;
        return true;
    }

    template <class Derived>
    auto Thread<Derived>::StopRunning() noexcept -> void {
        m_thread_running = false;
    }

    template <class Derived>
    auto Thread<Derived>::CloseThread() noexcept -> void {
        static_cast<Derived&>(*this).ImplementCloseThread();
    }
}