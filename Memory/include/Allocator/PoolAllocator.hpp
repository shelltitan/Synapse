#pragma once
#include <AlignmentUtility.hpp>
#include <Log.hpp>
#include <bit>
#include <cstddef>
#include <libassert/assert.hpp>

namespace Synapse::Memory::Allocator {
    /**
     * @brief Fixed-size free-list allocator for uniformly sized elements.
     *
     * Builds a linked list of equally sized blocks on construction and
     * provides O(1) allocate/deallocate by manipulating the head pointer.
     *
     * @tparam TMaxElementSizeInBytes Maximum size of each element the pool can serve.
     * @tparam TOffset                Extra bytes reserved before the user pointer.
     * @tparam TMaxAlignment          Maximum alignment guaranteed by the pool.
     */
    template<std::size_t TMaxElementSizeInBytes, std::size_t TOffset, std::size_t TMaxAlignment>
    class PoolAllocator {
    public:
        /**
         * @brief Constructs the pool over an existing memory range.
         * @param start Pointer to the first byte of the buffer.
         * @param end   Pointer one past the last byte of the buffer.
         */
        PoolAllocator(std::byte *start, std::byte *end) noexcept : m_start(start), m_end(end) { Reset(); }
        /**
         * @brief Constructs the pool using a buffer start and explicit size.
         * @param size  Number of bytes provided for the pool.
         * @param start Pointer to the first byte of the buffer.
         */
        PoolAllocator(const std::size_t size, std::byte *start) noexcept : m_start(start), m_end(start + size) {
            Reset();
        }
        ~PoolAllocator() = default;

        PoolAllocator(const PoolAllocator &) = delete;
        PoolAllocator(PoolAllocator &&) = delete;
        auto operator=(const PoolAllocator &) -> PoolAllocator & = delete;
        auto operator=(PoolAllocator &&) -> PoolAllocator & = delete;
        auto operator==(const PoolAllocator &other) const -> bool = delete;

        // Could make it memory safe with Compare exchange
        /**
         * @brief Retrieves a block from the pool.
         * @param size       Requested size (must not exceed `TMaxElementSizeInBytes`).
         * @param alignment  Requested alignment (must not exceed `TMaxAlignment`).
         * @return Pointer to the allocated block or nullptr if the pool is empty.
         */
        [[nodiscard]] auto Allocate([[maybe_unused]] const std::size_t size,
                [[maybe_unused]] const std::size_t alignment) noexcept -> std::byte * {
            DEBUG_ASSERT(std::has_single_bit(alignment) == true, "Invalid alignment. Must be power of two.");
            DEBUG_ASSERT(alignment <= TMaxAlignment,
                    "Alignment is bigger than expected maximum alignment for this pool");
            DEBUG_ASSERT(size <= TMaxElementSizeInBytes,
                    "Element size is bigger than expected maximum element size for this pool");
            DEBUG_ASSERT(size > 0u, "Allocation has to be at least 1 byte");

            if (m_current == nullptr) {
                CORE_DEBUG("Freelist out of memory!");
                return nullptr;
            }

            // obtain one element from the head of the free list
            std::byte *head = m_current;
            m_current = *std::bit_cast<std::byte **>(head);
            return (head - TOffset);
        }

        // Could make it memory safe with Compare exchange
        /**
         * @brief Returns a previously allocated block back to the pool.
         * @param ptr Pointer returned by Allocate (optionally offset by `TOffset`).
         */
        auto Deallocate(std::byte *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Cannot deallocate a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            // put the returned element at the head of the free list
            const auto list_ptr = ptr + TOffset;
            *(std::bit_cast<std::byte**>(list_ptr)) = m_current;
            m_current = list_ptr;
        }

        /**
         * @brief Reports the block size that this pool hands out.
         */
        [[nodiscard]] static auto GetAllocationSize([[maybe_unused]] const void *ptr) noexcept -> std::size_t {
            DEBUG_ASSERT(ptr != nullptr, "Cannot deallocate a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            return TMaxElementSizeInBytes;
        }

        /**
         * @brief Rebuilds the internal free list using the configured buffer.
         */
        auto Reset() noexcept -> void {
            static_assert(alignof(void *) <= TMaxAlignment);
            static_assert(std::has_single_bit(TMaxAlignment) == 0);
            static_assert(TMaxElementSizeInBytes >= sizeof(void *));

            std::byte *current = Utility::AlignAddress(m_start + TOffset, TMaxAlignment);

            if ((current + TMaxElementSizeInBytes) >= m_end) {
                m_current = nullptr;
                return;
            }

            auto next_node_ptr = std::bit_cast<std::byte**>(current);
            m_current = current;
            current += TMaxElementSizeInBytes;

            // initialise the free list - make every link point to the next element in the list
            while (true) {
                current = Utility::AlignAddress(current + TOffset, TMaxAlignment);
                if ((current + TMaxElementSizeInBytes) >= m_end) {
                    break;
                }
                *(next_node_ptr) = current;
                next_node_ptr = std::bit_cast<std::byte**>(current);
                current += TMaxElementSizeInBytes;
            }

            *(next_node_ptr) = nullptr;
        }

    private:
        std::byte *m_start;
        std::byte *m_end;
        std::byte *m_current{ nullptr };
    };
}
