#include "DeadlockProfiler.hpp"
#include <Log/Log.hpp>
#include <libassert/assert.hpp>
#include "ThreadLocals.hpp"

namespace CoreThread {
    void DeadLockProfiler::PushLock(const std::string& name) {
        std::scoped_lock lock{m_lock};

        // Find or issue an ID.
        std::int32_t lock_id = 0;

        auto find_iterator = m_name_to_id.find(name);
        if (find_iterator == m_name_to_id.end()) {
            lock_id = static_cast<std::int32_t>(m_name_to_id.size());
            m_name_to_id[name] = lock_id;
            m_id_to_name[lock_id] = name;
        }
        else {
            lock_id = find_iterator->second;
        }

        // If there was a lock to hold on to
        if (ThreadLocal::LockStack.empty() == false) {
            // If it is a case that has not been discovered previously, check again for deadlock.
            const std::int32_t prev_id = ThreadLocal::LockStack.top();
            if (lock_id != prev_id) {
                std::set<std::int32_t>& history = m_lock_history[prev_id];
                if (history.find(lock_id) == history.end()) {
                    history.insert(lock_id);
                    CheckCycle();
                }
            }
        }

        ThreadLocal::LockStack.push(lock_id);
    }

    void DeadLockProfiler::PopLock(const std::string& name) {
        std::scoped_lock lock{m_lock};

        DEBUG_ASSERT(!ThreadLocal::LockStack.empty(), "Trying to unlock lock multiple times");

        std::int32_t lock_id = m_name_to_id[name];

        DEBUG_ASSERT(ThreadLocal::LockStack.top() == lock_id, "Trying to unlock out of order");

        ThreadLocal::LockStack.pop();
    }

    void DeadLockProfiler::CheckCycle() {
        const std::int32_t lock_count = static_cast<std::int32_t>(m_name_to_id.size());
        m_discovered_order = std::vector<std::int32_t>(lock_count, -1);
        m_discovered_count = 0;
        m_finished = std::vector<bool>(lock_count, false);
        m_parent = std::vector<std::int32_t>(lock_count, -1);

        for (std::int32_t lock_id = 0; lock_id < lock_count; lock_id++) {
            Dfs(lock_id);
        }


        // When the computation is complete, clean up.
        m_discovered_order.clear();
        m_finished.clear();
        m_parent.clear();
    }

    void DeadLockProfiler::Dfs(std::int32_t here) {
        if (m_discovered_order[here] != -1)
            return;

        m_discovered_order[here] = m_discovered_count++;

        // Iterate through all adjacent vertices.
        auto find_iterator = m_lock_history.find(here);
        if (find_iterator == m_lock_history.end()) {
            m_finished[here] = true;
            return;
        }

        std::set<std::int32_t>& next_set = find_iterator->second;
        for (std::int32_t there : next_set) {
            // If you haven't visited yet, go visit.
            if (m_discovered_order[there] == -1) {
                m_parent[there] = here;
                Dfs(there);
                continue;
            }

            // If here was discovered before there, then there is a descendant of here. (forward trunk)
            if (m_discovered_order[here] < m_discovered_order[there]) {
                continue;
            }

            // If it is not forward, and Dfs(there) has not yet ended, then there is an ancestor of here. (reverse main line)
            if (m_finished[there] == false) {
                CORE_CRITICAL("{} -> {}", m_id_to_name[here], m_id_to_name[there]);

                std::int32_t now = here;
                while (true) {
                    CORE_CRITICAL("{} -> {}", m_id_to_name[m_parent[now]], m_id_to_name[now]);
                    now = m_parent[now];
                    if (now == there) {
                        break;
                    }
                }

                DEBUG_ASSERT(false, "Deadlock detected");
            }
        }

        m_finished[here] = true;
    }
}