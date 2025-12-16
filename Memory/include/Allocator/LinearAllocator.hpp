#pragma once
#include <AlignmentUtility.hpp>
#include <Log.hpp>

#include <libassert/assert.hpp>

#include <cstddef>
#include <utility>
#include <vadefs.h>

namespace Synapse::Memory::Allocator {
    /**
     * @brief Linear allocator (or bump allocator) for bulk allocations that are reset together.
     *
     * Grows forward within a fixed buffer and supports very fast allocations with no
     * per-object deallocation. Each allocation advances a cursor; `Reset()` rewinds
     * the cursor to the start of the buffer and invalidates all outstanding
     * allocations.
     *
     * Allocation is typically: align the current cursor, compute the returned address,
     * then advance the cursor by the requested size (plus any reserved prefix).
     *
     * This allocator has low overhead because it maintains only a cursor (no free lists
     * or per-allocation bookkeeping). Internal fragmentation is limited to alignment
     * padding between allocations.
     *
     * Use when lifetimes are uniform and you can discard all allocations at once.
     * 
     * @tparam TOffset Number of bytes reserved immediately before the returned user pointer
     *                 (e.g., for a small header, metadata, or back-pointer).
     */
    template <std::size_t TOffset>
    class LinearAllocator {
    public:
        LinearAllocator() = delete;
        /**
         * @brief Constructs the allocator over an existing memory span.
         * @param start Pointer to the first byte of the buffer.
         * @param end   Pointer one past the last byte of the buffer.
         */
        LinearAllocator(std::byte* start, std::byte* end) noexcept : m_start(start), m_end(end), m_current(start) {};
        /**
         * @brief Constructs the allocator with explicit size and start pointer.
         * @param size  Number of bytes to manage.
         * @param start Pointer to the first byte of the buffer.
         */
        LinearAllocator(const std::size_t size, std::byte* start) noexcept : m_start(start), m_end(start + size), m_current(start) {};
        ~LinearAllocator() = default;

        LinearAllocator(const LinearAllocator&) = delete;
        LinearAllocator(LinearAllocator&&) = delete;
        auto operator=(const LinearAllocator &) -> LinearAllocator & = delete;
        auto operator=(LinearAllocator &&) -> LinearAllocator & = delete;
        auto operator==(const LinearAllocator &other) const -> bool = delete;

        /**
         * @brief Allocates a contiguous block from the current cursor.
         * @param size       Bytes requested.
         * @param alignment  Alignment requirement; must be a power of two.
         * @return Pointer to aligned memory or nullptr when the buffer is exhausted.
         */
        [[nodiscard]] auto Allocate(const std::size_t size,
                const std::size_t alignment) noexcept -> std::byte * {
            DEBUG_ASSERT(std::has_single_bit(alignment) == 0, "Invalid alignment. Must be power of two.");
            DEBUG_ASSERT(size >= 0, "Allocation has to be at least 1 byte");

            std::byte* reset_ptr{ m_current };
            // offset the pointer first, align it, and offset it back
            m_current = Utility::AlignAddress(m_current + TOffset + sizeof(std::size_t), alignment) - (TOffset + sizeof(std::size_t));

            if ((m_current + size + TOffset + sizeof(std::size_t)) >= m_end) {
                CORE_DEBUG("LinearAllocator out of memory!");
                m_current = reset_ptr;
                return nullptr;
            }

            (void)std::copy_n(m_current, sizeof(std::size_t), std::bit_cast<std::byte*>(&size));

            return m_current + sizeof(std::size_t);
        }

        /**
         * @brief Linear allocator does not support individual deallocations.
         *
         * Always triggers a debug assertion; use `Reset()` to reclaim memory.
         */
        auto Deallocate([[maybe_unused]] void *ptr) noexcept -> void {
            DEBUG_ASSERT(false, "Linear allocator is meant to be reset and no freeing is expected.");
            std::unreachable();
        }

        /**
         * @brief Rewinds the allocator to the start of its buffer.
         */
        auto Reset() noexcept -> void { m_current = m_start; }

        /**
         * @brief Returns total managed capacity in bytes.
         */
        [[nodiscard]] auto GetSize() const noexcept -> std::size_t {
            return std::bit_cast<uintptr_t>(m_end) - std::bit_cast<uintptr_t>(m_start);
        };
        /**
         * @brief Reports bytes consumed from the buffer.
         */
        [[nodiscard]] auto GetUsed() const noexcept -> std::size_t {
            return std::bit_cast<uintptr_t>(m_current) - std::bit_cast<uintptr_t>(m_start);
        };
        /**
         * @brief Gives access to the raw buffer start pointer.
         */
        [[nodiscard]] auto GetStart() const noexcept -> const std::byte * { return m_start; };
        /**
         * @brief Reads the stored size of a prior allocation.
         * @param ptr Pointer returned by Allocate.
         */
        [[nodiscard]] auto GetAllocationSize(const std::byte *ptr) const noexcept -> std::size_t {
            DEBUG_ASSERT(ptr != nullptr, "Cannot get allocation size of a null pointer");
            std::size_t size{};
            (void)std::copy_n(ptr - (sizeof(std::size_t) + TOffset), sizeof(std::size_t), std::bit_cast<std::byte*>(&size));
            return size;
        };

    private:
        std::byte* m_start;
        std::byte* m_end;
        std::byte* m_current;
    };
}
