#pragma once
#include <array>
#include <bit>
#include <bitset>
#include <memory>
#include <new>
#include <vector>

namespace Synapse {
    namespace STL {
        // This is not thread safe, if you use it in multithreaded context you need to manage locking
        template <class TType, std::size_t TCount>
        class ObjectPool {
        public:
            template <class... Args>
            explicit ObjectPool(Args&&... args) noexcept
                : m_last_free_block_index(TCount - 1), m_free_index_queue{}, m_pool{}, m_status{} {
                m_pool.fill(TType{ std::forward<Args>(args)... });
                m_status.reset();
                for (std::size_t i = 0; i < TCount; ++i) {
                    m_free_index_queue[i] = i;
                }
            }

            template <class Iterator>
            ObjectPool(Iterator begin, Iterator end) noexcept
                : m_last_free_block_index(TCount - 1), m_free_index_queue{}, m_pool{}, m_status{} {
                static_assert(TCount == std::distance(begin, end));

                std::move(begin, end, m_pool.begin());
                m_status.reset();
                for (std::size_t i = 0; i < TCount; ++i) {
                    m_free_index_queue[i] = i;
                }
            }

            ObjectPool(std::initializer_list<TType> init) noexcept
                : m_last_free_block_index(TCount - 1), m_free_index_queue{}, m_pool{}, m_status{} {
                static_assert(TCount == init.size());

                (void)std::copy_n(init.begin(), init.end(), m_pool.begin());
                m_status.reset();
                for (std::size_t i = 0; i < TCount; ++i) {
                    m_free_index_queue[i] = i;
                }
            }

            ObjectPool(const ObjectPool& other) = delete;
            ObjectPool(ObjectPool&& other) = delete;
            auto operator=(const ObjectPool &other) -> ObjectPool & = delete;
            auto operator=(ObjectPool &&other) -> ObjectPool & = delete;

            auto Pop() noexcept -> TType * {
                if (m_last_free_block_index == std::numeric_limits<std::size_t>::max()) {
                    return nullptr;
                }

                std::size_t index = m_free_index_queue[m_last_free_block_index];
                DEBUG_ASSERT(!m_status[index]);
                m_status.set(index);
                --m_last_free_block_index;
                return &m_pool[index];
            }

            auto Push(TType *object) noexcept -> void {
                DEBUG_ASSERT(nullptr != object);
                DEBUG_ASSERT(!std::less<TType*>{}(object, m_pool.data()));
                DEBUG_ASSERT(!std::greater<TType*>{}(object, m_pool.data() + m_pool.size()));
                DEBUG_ASSERT(((std::bit_cast<std::byte*>(object) - std::bit_cast<std::byte*>(m_pool.data())) % sizeof(object)) == 0);
                std::size_t index = object - m_pool.data();
                if (m_status[index]) {
                    m_status.reset(index);
                    m_free_index_queue[++m_last_free_block_index] = index;
                }
            }

            auto GetNumberOfAvailableObjects() const noexcept -> std::size_t {
                return m_last_free_block_index + 1;
            }

        private:
            std::size_t m_last_free_block_index;
            alignas(std::hardware_destructive_interference_size) std::array<std::size_t, TCount> m_free_index_queue;
            alignas(std::hardware_destructive_interference_size) std::array<TType, TCount> m_pool;
            alignas(std::hardware_destructive_interference_size) std::bitset<TCount> m_status;
        };
    }
    namespace Runtime {
        /// \todo add allocator, make it work with the allocators
        template <class T>
        class ObjectPool {
        public:
            ObjectPool(int size = 100) {
                // m_pool.resize(size);
            }

            auto IsEmpty() const -> bool {
                // return m_pool.empty();
            }

            auto GetSize() const -> std::size_t {
                // return m_pool.size();
            }

            auto Resize(std::size_t size) -> void {
                // m_pool.resize(size);
            }

            auto Clear() -> void {
                // m_pool.clear();
            }

            auto Set(std::size_t index, T object) -> void {
                // m_pool[index] = object;
            }
        };
    }
}