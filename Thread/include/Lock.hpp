#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace CoreThread {
    /*--------------------------------------------
    [WWWWWWWW][WWWWWWWW][RRRRRRRR][RRRRRRRR]
    W : WriteFlag (Exclusive Lock Owner thread_id)
    R : ReadFlag (Shared Lock Count)
    ---------------------------------------------*/

    class Lock {
        const std::chrono::milliseconds ACQUIRE_TIMEOUT_TICK = std::chrono::milliseconds(10000);
        const std::uint32_t MAX_SPIN_COUNT = 5000;
        const std::uint32_t WRITE_THREAD_MASK = 0xFFFF'0000;
        const std::uint32_t READ_COUNT_MASK = 0x0000'FFFF;
        const std::uint32_t EMPTY_FLAG = 0x0000'0000;

    public:
        auto WriteLock(const std::string &name) -> void;
        auto WriteUnlock(const std::string &name) -> void;
        auto ReadLock(const std::string &name) -> void;
        auto ReadUnlock(const std::string &name) -> void;

    private:
        std::atomic<std::uint32_t> m_lock_flag = EMPTY_FLAG;
        std::uint16_t m_write_count = 0;
    };

    class ReadLockGuard {
    public:
        ReadLockGuard(Lock& lock, const std::string& name) : m_lock(lock), m_name(name) { m_lock.ReadLock(name); }
        ~ReadLockGuard() { m_lock.ReadUnlock(m_name); }

    private:
        Lock& m_lock;
        const std::string m_name;
    };

    class WriteLockGuard {
    public:
        WriteLockGuard(Lock& lock, const std::string& name) : m_lock(lock), m_name(name) { m_lock.WriteLock(name); }
        ~WriteLockGuard() { m_lock.WriteUnlock(m_name); }

    private:
        Lock& m_lock;
        const std::string m_name;
    };
}

#define USE_MANY_LOCKS(count) CoreThread::Lock _locks[count];
#define USE_LOCK USE_MANY_LOCKS(1)
#define READ_LOCK_IDX(idx) CoreThread::ReadLockGuard readLockGuard_##idx(_locks[idx], typeid(this).name());
#define READ_LOCK READ_LOCK_IDX(0)
#define WRITE_LOCK_IDX(idx) CoreThread::WriteLockGuard writeLockGuard_##idx(_locks[idx], typeid(this).name());
#define WRITE_LOCK WRITE_LOCK_IDX(0)