#pragma once
#include <functional>
#include <memory>


namespace CoreThread::Job {
    class Job {
    public:
        explicit Job(std::function<void()>&& callback) : m_callback(std::move(callback)) {
        }

        template <typename T, typename Ret, typename... Args>
        Job(std::shared_ptr<T> owner, Ret (T::*memFunc)(Args...), Args&&... args) {
            m_callback = [owner, memFunc, args...]() {
                (owner.get()->*memFunc)(args...);
            };
        }

        auto Execute() const -> void {
            m_callback();
        }

    private:
        std::function<void()> m_callback;
    };
}
