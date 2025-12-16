#pragma once
#include <AlignmentUtility.hpp>
#include <Log.hpp>
#include <bit>
#include <cstddef>
#include <libassert/assert.hpp>


namespace Synapse::Memory::Allocator {
    /**
     * @brief Free-list allocator for variable sized allocations.
     *
     * Maintains a singly linked list of free blocks and serves requests by
     * either picking the best-fitting or first-fitting block depending on
     * `TBestFit`. All allocations store a small header to support size queries
     * and deallocation.
     *
     * @tparam TOffset   Number of extra bytes reserved before the returned user pointer.
     * @tparam TBestFit  When `true`, searches for the smallest fitting free block;
     *                   otherwise uses the first fitting block.
     */
    template<std::size_t TOffset, bool TBestFit = true>
    class FreeListAllocator {
        struct NodeHeader {
            std::size_t node_size;
            std::byte *next_node_ptr;
        };
        struct AllocationHeader {
            std::size_t allocation_size;
            std::byte *allocation_reset_ptr;
        };

    public:
        FreeListAllocator() = delete;
        /**
         * @brief Initializes the allocator with a pre-reserved memory range.
         * @param start Pointer to the first byte of the managed range.
         * @param end   Pointer one past the last byte of the managed range.
         */
        FreeListAllocator(std::byte *start, std::byte *end) noexcept : m_start(start), m_end(end), m_current(m_start) {
            const NodeHeader header{ .node_size = std::bit_cast<uintptr_t>(end) - std::bit_cast<uintptr_t>(start),
                .next_node_ptr = nullptr };
            // Store header
            (void)std::copy_n(std::bit_cast<const std::byte *>(&header), sizeof(NodeHeader), m_current);
        }
        /**
         * @brief Initializes the allocator with a start pointer and buffer size.
         * @param size  Number of bytes to manage.
         * @param start Pointer to the first byte of the buffer.
         */
        FreeListAllocator(const std::size_t size, std::byte *start) noexcept :
            m_start(start), m_end(start + size), m_current(m_start) {
            const NodeHeader header{ .node_size = size, .next_node_ptr = nullptr };
            // Store header
            (void)std::copy_n(std::bit_cast<const std::byte *>(&header), sizeof(NodeHeader), m_current);
        };
        ~FreeListAllocator() = default;

        FreeListAllocator(const FreeListAllocator &) = delete;
        FreeListAllocator(FreeListAllocator &&) = delete;
        auto operator=(const FreeListAllocator &) -> FreeListAllocator & = delete;
        auto operator=(FreeListAllocator &&) -> FreeListAllocator & = delete;
        auto operator==(const FreeListAllocator &other) const -> bool = delete;

        /**
         * @brief Allocates a block of memory from the free list.
         * @param allocation_size Size of the requested block in bytes.
         * @param alignment       Desired alignment; must be a power of two.
         * @return Pointer to the aligned memory block or nullptr on failure.
         */
        [[nodiscard]] auto Allocate(const std::size_t allocation_size, const std::size_t alignment) noexcept -> std::byte * {
            DEBUG_ASSERT(std::has_single_bit(alignment) == 0, "Invalid alignment. Must be power of two.");
            DEBUG_ASSERT(allocation_size >= 0, "Allocation has to be at least 1 byte");

            // Find the best fitting slot approach
            if constexpr (TBestFit) {
                std::size_t smallest_difference = std::numeric_limits<std::size_t>::max();
                const std::byte *node_before_best_node = nullptr;
                std::byte *best_node = nullptr;

                {
                    const std::byte *previous_node = nullptr;
                    std::byte *current_node = m_current;

                    while (current_node != nullptr) {
                        NodeHeader header{};
                        (void)std::copy_n(current_node, sizeof(NodeHeader), static_cast<std::byte *>(&header));
                        // Offset the pointer first, align it, and then offset it back
                        std::byte *shifted_ptr =
                                Utility::AlignAddress(current_node + sizeof(AllocationHeader) + TOffset, alignment) -
                                (sizeof(AllocationHeader) + TOffset);
                        std::size_t required_size = allocation_size + (std::bit_cast<uintptr_t>(shifted_ptr) -
                                                                   std::bit_cast<uintptr_t>(current_node));
                        if (const std::size_t remaining_size = header.node_size - required_size; (header.node_size >= required_size) && (remaining_size < smallest_difference)) {
                            node_before_best_node = previous_node;
                            best_node = current_node;
                            smallest_difference = remaining_size;
                            if (remaining_size == 0U) {
                                break;
                            }
                        }
                        previous_node = current_node;
                        current_node = header.next_node_ptr;
                    }
                }
                if (best_node != nullptr) {
                    NodeHeader header{};
                    (void)std::copy_n(best_node, sizeof(NodeHeader), static_cast<std::byte *>(&header));
                    // Offset the pointer first, align it, and then offset it back
                    std::byte *shifted_ptr = Utility::AlignAddress(best_node + sizeof(AllocationHeader) + TOffset, alignment) -
                                             (sizeof(AllocationHeader) + TOffset);
                    return ObtainNode(smallest_difference, allocation_size, shifted_ptr, node_before_best_node, best_node,
                            header.next_node_ptr);
                } else {
                    CORE_DEBUG("FreeListAllocator out of memory!");
                    return nullptr;
                }
            }
            // Find the first fitting slot approach
            else {
                const std::byte *previous_node = nullptr;
                std::byte *current_node = m_current;
                while (current_node != nullptr) {
                    NodeHeader header{};
                    (void)std::copy_n(current_node, sizeof(NodeHeader), static_cast<std::byte *>(&header));
                    // Offset the pointer first, align it, and then offset it back
                    std::byte *shifted_ptr =
                            Utility::AlignAddress(current_node + sizeof(AllocationHeader) + TOffset, alignment) -
                            (sizeof(AllocationHeader) + TOffset);
                    std::size_t required_size = allocation_size + (std::bit_cast<uintptr_t>(shifted_ptr) -
                                           std::bit_cast<uintptr_t>(current_node));
                    if (header.node_size >= required_size) {
                        return ObtainNode(
                                header.node_size - required_size, allocation_size, shifted_ptr, previous_node, current_node, header.next_node_ptr);
                    }

                    previous_node = current_node;
                    current_node = header.next_node_ptr;
                }
                CORE_DEBUG("FreeListAllocator out of memory!");
                return nullptr;
            }
        }

        /**
         * @brief Returns a previously allocated block to the free list.
         * @param ptr Pointer returned by Allocate (optionally offset by `TOffset`).
         */
        auto Deallocate(std::byte *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Cannot deallocate a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            AllocationHeader allocation_header{};
            (void)std::copy_n(ptr - sizeof(AllocationHeader), sizeof(AllocationHeader), static_cast<std::byte *>(&allocation_header));

            std::byte *previous_node = nullptr;
            std::byte *current_node = m_current;
            while (current_node != nullptr) {
                NodeHeader node_header{};
                (void)std::copy_n(current_node, sizeof(NodeHeader), static_cast<std::byte *>(&node_header));
                if (allocation_header.allocation_reset_ptr < current_node) {
                    break;
                }
                previous_node = current_node;
                current_node = node_header.next_node_ptr;
            }

            NodeHeader new_node{};
            new_node.node_size = allocation_header.allocation_size;
            if ((static_cast<const std::byte *>(ptr) + TOffset + allocation_header.allocation_size) == current_node) {
                NodeHeader node{};
                (void)std::copy_n(current_node, sizeof(NodeHeader), static_cast<std::byte *>(&node));
                new_node.next_node_ptr = node.next_node_ptr;
                new_node.node_size += node.node_size;
            } else {
                new_node.next_node_ptr = current_node;
            }
            if (previous_node != nullptr) {
                NodeHeader node{};
                (void)std::copy_n(current_node, sizeof(NodeHeader), static_cast<std::byte *>(&node));
                if ((previous_node + node.node_size) == allocation_header.allocation_reset_ptr) {
                    new_node += node.node_size;
                    (void)std::copy_n(static_cast<std::byte *>(&new_node), sizeof(NodeHeader), previous_node);
                } else {
                    node.next_node_ptr = allocation_header.allocation_reset_ptr;
                    (void)std::copy_n(static_cast<std::byte *>(&new_node), sizeof(NodeHeader), allocation_header.allocation_reset_ptr);
                }
            } else {
                m_current = allocation_header.allocation_reset_ptr;
                (void)std::copy_n(static_cast<std::byte *>(&new_node), sizeof(NodeHeader), m_current);
            }
        }

        /**
         * @brief Retrieves the size of an allocation previously returned by Allocate.
         * @param ptr Pointer inside the allocated block.
         * @return Size in bytes of the allocation.
         */
        [[nodiscard]] auto GetAllocationSize(std::byte *ptr) const noexcept -> const std::size_t {
            DEBUG_ASSERT(ptr != nullptr, "Cannot get allocation size of a null pointer");
            DEBUG_ASSERT((ptr >= m_start) && (ptr < m_end), "Pointer was not allocated by the alloctor");
            AllocationHeader header{};
            (void)copy_n(ptr - (sizeof(AllocationHeader) + TOffset), sizeof(AllocationHeader), static_cast<std::byte *>(&header));
            return header.allocation_size;
        }

        /**
         * @brief Resets the allocator to its initial state, invalidating all outstanding allocations.
         */
        auto Reset() noexcept -> void {
            const NodeHeader header{ .node_size =
                                                   std::bit_cast<uintptr_t>(m_end) - std::bit_cast<uintptr_t>(m_start),
                .next_node_ptr = nullptr };
            m_current = m_start;
            (void)std::copy_n(std::bit_cast<const std::byte *>(&header), sizeof(AllocationHeader), m_current);
        }

    private:
        /**
         * @brief Updates the freelist links after removing or splitting a node.
         * @param next_node     Pointer to the node that should follow `previous_node`.
         * @param previous_node Pointer to the node that precedes the updated position; may be nullptr when updating the head.
         */
        auto AdjustLinkedList(std::byte *next_node, std::byte *previous_node) -> void {
            if (previous_node != nullptr) {
                NodeHeader header_previous{};
                (void)std::copy_n(
                        previous_node, sizeof(NodeHeader), static_cast<std::byte *>(&header_previous));
                header_previous.next_node_ptr = next_node;
                (void)std::copy_n(static_cast<std::byte *>(&header_previous), sizeof(NodeHeader), previous_node);
            } else {
                // we are at the start of the list, so we have to set the m_current pointer to the next node
                m_current = next_node;
            }
        }

        /**
         * @brief Writes allocation metadata and optionally splits a node to satisfy a request.
         * @param remaining_size Bytes left in the free block after the allocation.
         * @param size            Requested allocation size.
         * @param shifted_ptr     Pointer aligned for the allocation payload.
         * @param previous_node   Node that precedes the allocated block in the freelist; may be nullptr.
         * @param current_node    Node being consumed.
         * @param next_node       Node that originally followed `current_node`.
         * @return Pointer to the user-accessible memory region.
         */
        auto ObtainNode(const std::size_t remaining_size, const std::size_t size, std::byte *shifted_ptr,
                const std::byte *previous_node, const std::byte *current_node,
                const std::byte *next_node) -> std::byte * {
            // Break the node into two parts if the remaining size is big enough
            AllocationHeader header{};
            header.allocation_reset_ptr = current_node;
            if (remaining_size >= sizeof(NodeHeader)) {
                header.allocation_size = size;
                (void)std::copy_n(std::bit_cast<std::byte *>(&header), sizeof(AllocationHeader), shifted_ptr);

                NodeHeader new_node{};
                new_node.next_node_ptr = next_node;
                new_node.node_size = remaining_size;
                std::byte *new_node_ptr{ shifted_ptr + sizeof(AllocationHeader) + TOffset + size };
                (void)std::copy_n(static_cast<std::byte *>(&new_node), sizeof(NodeHeader), new_node_ptr);
                AdjustLinkedList(new_node_ptr, previous_node);
            } else {
                header.allocation_size += size + remaining_size;
                (void)std::copy_n(std::bit_cast<std::byte *>(&header), sizeof(AllocationHeader), shifted_ptr);

                AdjustLinkedList(next_node, previous_node);
            }
            return shifted_ptr + sizeof(AllocationHeader);
        }

        std::byte *m_start;
        std::byte *m_end;
        std::byte *m_current;
    };
}
