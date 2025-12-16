#include "ThreadLocals.hpp"

namespace CoreThread::ThreadLocal {
    thread_local std::uint32_t thread_id = 0;
    thread_local std::chrono::time_point<std::chrono::steady_clock> end_tick_count;
    thread_local std::stack<std::int32_t> LockStack;
    thread_local Job::JobQueue* CurrentJobQueue = nullptr;
}
