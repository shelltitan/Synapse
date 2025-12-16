#pragma once
#include <map>
#include <mutex>
#include <set>
#include <stack>
#include <vector>
#include <ankerl/unordered_dense.h>

namespace CoreThread {

    class DeadLockProfiler {
    public:
        auto PushLock(const std::string &name) -> void;
        auto PopLock(const std::string &name) -> void;
        auto CheckCycle() -> void;

    private:
        auto Dfs(std::int32_t index) -> void;

        ankerl::unordered_dense::map<std::string, std::int32_t> m_name_to_id;
        ankerl::unordered_dense::map<std::int32_t, std::string> m_id_to_name;
        std::map<std::int32_t, std::set<std::int32_t>> m_lock_history;

        std::mutex m_lock;

        std::vector<std::int32_t> m_discovered_order; // Array recording the order in which nodes were found
        std::int32_t m_discovered_count = 0;          // Order in which nodes are discovered
        std::vector<bool> m_finished;                 // Whether Dfs(i) has terminated
        std::vector<std::int32_t> m_parent;
    };
}

namespace Global {
#if _DEBUG
    inline std::unique_ptr<CoreThread::DeadLockProfiler> GDeadLockProfiler = std::make_unique<CoreThread::DeadLockProfiler>();
#endif
}