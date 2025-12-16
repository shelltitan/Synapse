#pragma once
#include <AlignmentUtility.hpp>
#include <Log.hpp>
#include <libassert/assert.hpp>

#include <bit>
#include <cstddef>

namespace Synapse::Memory::Allocator {
    /**
     * @brief Simple bump-pointer allocator with optional LIFO validation.
     *
     * Grows linearly within a fixed buffer and releases memory only via
     * `Reset()`. When `STACK_LIFO_CHECK` is enabled, deallocations must occur
     * strictly in reverse order of allocations.
     *
     * Memory layout:
     * Stack ID | Allocation size | Reset pointer | Cannary Front | User Memory | Cannary Back
     *
     * @tparam TOffset Number of bytes reserved before the returned user pointer.
     */
    template <std::size_t TOffset>
    class StackAllocator {
        struct AllocationHeader {
#ifdef STACK_LIFO_CHECK
                    std::size_t stack_lifo_id;
#endif
            std::size_t allocation_size;
            std::byte* allocation_reset_ptr;
        };

    public:
        StackAllocator() = delete;
        /**
         * @brief Constructs the allocator over an existing memory range.
         * @param start Pointer to the first byte of the buffer.
         * @param end   Pointer one past the last byte of the buffer.
         */
        StackAllocator(std::byte* start, std::byte* end) noexcept : m_start(start), m_end(end), m_current(start) {};
        /**
         * @brief Constructs the allocator with a buffer start and explicit size.
         * @param size  Number of bytes managed by the allocator.
         * @param start Pointer to the first byte of the buffer.
         */
        StackAllocator(const std::size_t size, std::byte* start) noexcept : m_start(start), m_end(start + size), m_current(start) {};
        ~StackAllocator() = default;

        StackAllocator(const StackAllocator&) = delete;
        StackAllocator(StackAllocator&&) = delete;
        auto operator=(const StackAllocator &) -> StackAllocator & = delete;
        auto operator=(StackAllocator &&) -> StackAllocator & = delete;
        auto operator==(const StackAllocator &other) const -> bool = delete;

        /**
         * @brief Allocates memory from the top of the stack.
         * @param size       Number of bytes requested.
         * @param alignment  Desired alignment; must be a power of two.
         * @return Pointer to aligned memory or nullptr when out of space.
         */
        [[nodiscard]] inline auto Allocate(const std::size_t size,
                const std::size_t alignment) noexcept -> std::byte * {
            DEBUG_ASSERT(std::has_single_bit(alignment) == 0, "Invalid alignment. Must be power of two.");
            DEBUG_ASSERT(size >= 0, "Allocation has to be at least 1 byte");
            const AllocationHeader header{
#ifdef STACK_LIFO_CHECK
                        .stack_lifo_id = m_lifo_check_count + 1,
#endif
                .allocation_size = size,
                .allocation_reset_ptr = m_current
            };

            // Offset the pointer first, align it, and then offset it back
            m_current = Utility::AlignAddress(m_current + sizeof(AllocationHeader) + TOffset, alignment) - (sizeof(AllocationHeader) + TOffset);

            if ((m_current + size + sizeof(AllocationHeader) + TOffset) > m_end) {
                CORE_DEBUG("StackAllocator out of memory!");
                m_current = header.allocation_reset_ptr;
                return nullptr;
            }

#ifdef STACK_LIFO_CHECK
                    ++m_lifo_check_count;
#endif
            // Store header
            (void)std::copy_n(std::bit_cast<const std::byte*>(&header), sizeof(AllocationHeader), m_current);

            std::byte* user_ptr = m_current + sizeof(AllocationHeader);
            m_current += (size + TOffset + sizeof(AllocationHeader));
            return user_ptr;
        }

        // If memory had an offset, we expect that offset to be removed before returning the pointer
        /**
         * @brief Pops the most recent allocation off the stack.
         * @param ptr Pointer returned by Allocate.
         *
         * In debug builds with `STACK_LIFO_CHECK`, this asserts that the call
         * order respects the LIFO discipline.
         */
        inline auto Deallocate(const void *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Cannot deallocate a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            AllocationHeader allocation_header{};
            (void)std::copy_n(static_cast<const std::byte*>(ptr) - sizeof(AllocationHeader), sizeof(AllocationHeader), std::bit_cast<std::byte*>(&allocation_header));
#ifdef STACK_LIFO_CHECK
                    ASSERT(allocation_header.stack_lifo_id == m_lifo_check_count, "Stack deallacation must be LIFO order.");
                    --m_lifo_check_count;
#endif
            m_current = allocation_header.allocation_reset_ptr;
        }

        /**
         * @brief Returns the requested size for a previous allocation.
         * @param ptr Pointer returned by Allocate.
         */
        [[nodiscard]] auto GetAllocationSize(std::byte *ptr) const noexcept -> std::size_t {
            DEBUG_ASSERT(ptr != nullptr, "Cannot get allocation size of a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            std::byte* shifted_ptr = ptr - (sizeof(AllocationHeader) + TOffset);
            AllocationHeader allocation_header{};
            (void)std::copy_n(shifted_ptr, sizeof(AllocationHeader), std::bit_cast<std::byte*>(&allocation_header));
            return allocation_header.allocation_size;
        }

        /**
         * @brief Resets the allocator to the bottom of the stack, invalidating all allocations.
         */
        inline auto Reset() noexcept -> void {
            m_current = m_start;
#ifdef STACK_LIFO_CHECK
            m_lifo_check_count = 0;
#endif
        }

    private:
        std::byte* m_start;
        std::byte* m_end;
        std::byte* m_current;
#ifdef STACK_LIFO_CHECK
        std::size_t m_lifo_check_count{ 0 };
#endif
    };
}
