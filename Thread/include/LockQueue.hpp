#pragma once
#include <queue>
#include "Lock.h"

namespace CoreThread {
    template <typename T>
    class LockQueue {
    public:
        void Push(T item) {
            WRITE_LOCK;
            m_items.push(item);
        }

        T Pop() {
            WRITE_LOCK;
            if (m_items.empty()) {
                return T();
            }

            T ret = m_items.front();
            m_items.pop();
            return ret;
        }

        // These will write in items
        void PopAll(CoreMemory::Vector<T>& items) {
            WRITE_LOCK;
            while (T item = Pop()) {
                items.push_back(item);
            }
        }

        void Clear() {
            WRITE_LOCK;
            m_items = CoreMemory::Queue<T>();
        }

    private:
        USE_LOCK;
        CoreMemory::Queue<T> m_items;
    };
}