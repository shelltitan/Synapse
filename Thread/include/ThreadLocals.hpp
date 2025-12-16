#pragma once
#include <chrono>
#include <stack>

namespace CoreThread {
    namespace Job {
        class JobQueue;
    }
    namespace ThreadLocal {
        extern thread_local std::uint32_t thread_id;
        extern thread_local std::chrono::time_point<std::chrono::steady_clock> end_tick_count;
        extern thread_local std::stack<std::int32_t> LockStack;
        extern thread_local Job::JobQueue* CurrentJobQueue;
    }
}