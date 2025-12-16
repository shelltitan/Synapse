#pragma once
#include <cstddef>
#include <concepts>

namespace Synapse::Memory::Arena {
    /**
     * @brief Concept describing allocator requirements used by `MemoryArena`.
     *
     * A valid allocation policy must provide `Allocate` and `Deallocate`
     * functions that operate on raw byte pointers.
     */
    template <typename TPolicy>
    concept AllocationPolicy = requires(const TPolicy p, std::byte* memory, const std::size_t size, const std::size_t alignment) {
        { p.Allocate(size, alignment) } -> std::same_as<std::byte*>;
        p.Deallocate(memory);
    };

    /**
     * @brief Concept describing the backing storage required by `MemoryArena`.
     *
     * A valid area exposes start and end pointers alongside the total size of
     * the managed buffer.
     */
    template <typename TPolicy>
    concept AreaPolicy = requires(const TPolicy p) {
        { p.GetStart() } -> std::convertible_to<std::byte*>;
        { p.GetEnd() } -> std::convertible_to<std::byte*>;
        { p.GetSize() } -> std::convertible_to<std::size_t>;
    };
}
