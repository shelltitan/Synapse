#include "Lock.h"
#include "DeadLockProfiler.h"
#include "ThreadLocals.h"

#include <Log.h>
#include <libassert/assert.hpp>

namespace CoreThread {
    void Lock::WriteLock(const std::string& name) {
#if _DEBUG
        Global::GDeadLockProfiler->PushLock(name);
#endif

        // If owned by the same thread, success is guaranteed.
        const std::uint32_t lock_thread_id = (m_lock_flag.load() & WRITE_THREAD_MASK) >> 16;
        if (ThreadLocal::thread_id == lock_thread_id) {
            ++m_write_count;
            return;
        }

        // When no one owns or shares something, it competes for ownership.
        const std::chrono::time_point<std::chrono::steady_clock> begin_tick = std::chrono::steady_clock::now();
        const std::uint32_t desired = ((ThreadLocal::thread_id << 16) & WRITE_THREAD_MASK);
        while (true) {
            for (std::uint32_t spin_count = 0; spin_count < MAX_SPIN_COUNT; ++spin_count) {
                std::uint32_t expected = EMPTY_FLAG;
                if (m_lock_flag.compare_exchange_strong(expected, desired)) {
                    ++m_write_count;
                    return;
                }
            }

            DEBUG_ASSERT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_tick) < ACQUIRE_TIMEOUT_TICK, "Lock timed out. Thread has not been able to acquire lock in 10s!");

            std::this_thread::yield();
        }
    }

    void Lock::WriteUnlock(const std::string& name) {
#if _DEBUG
        Global::GDeadLockProfiler->PopLock(name);
#endif

        DEBUG_ASSERT((m_lock_flag.load() & READ_COUNT_MASK) == 0, "WriteUnlock is not possible until ReadLock is unlocked.");

        const std::int32_t lock_count = --m_write_count;
        if (lock_count == 0) {
            m_lock_flag.store(EMPTY_FLAG);
        }
    }

    void Lock::ReadLock(const std::string& name) {
#if _DEBUG
        Global::GDeadLockProfiler->PushLock(name);
#endif

        // If owned by the same thread, success is guaranteed.
        const std::uint32_t lock_thread_id = (m_lock_flag.load() & WRITE_THREAD_MASK) >> 16;
        if (ThreadLocal::thread_id == lock_thread_id) {
            m_lock_flag.fetch_add(1);
            return;
        }

        // When no one owns something, it competes to raise the share count.
        const std::chrono::time_point<std::chrono::steady_clock> begin_tick = std::chrono::steady_clock::now();
        while (true) {
            for (std::uint32_t spin_count = 0; spin_count < MAX_SPIN_COUNT; ++spin_count) {
                std::uint32_t expected = (m_lock_flag.load() & READ_COUNT_MASK);
                if (m_lock_flag.compare_exchange_strong(expected, expected + 1))
                    return;
            }

            // LOCK_TIMEOUT
            DEBUG_ASSERT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin_tick) < ACQUIRE_TIMEOUT_TICK, "Lock timed out. Thread has not been able to acquire lock in 10s!");

            std::this_thread::yield();
        }
    }

    void Lock::ReadUnlock(const std::string& name) {
#if _DEBUG
        Global::GDeadLockProfiler->PopLock(name);
#endif
        DEBUG_ASSERT((m_lock_flag.fetch_sub(1) & READ_COUNT_MASK) != 0, "Trying to unlock the same lock multiple times");
    }
}