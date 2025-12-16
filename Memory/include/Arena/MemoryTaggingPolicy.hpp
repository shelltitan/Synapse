#pragma once
#include <cstddef>
#include <concepts>

namespace Synapse::Memory::Arena {
    /*
     * Useful for finding dangling pointers use a certain value at allocation and change it to an other a deallocation
     * Options:
     * On or off
    */
    /// \todo The Visual Studio Debug Heap implementation marks newly allocated heap memory with 0xCDCDCDCD, and freed memory with 0xDDDDDDDD.
    /// \todo 0xDC fill pattern, destroy pattern 0xEF
    /**
     * @brief Concept for policies that paint allocations/deallocations with patterns.
     */
    template <typename TPolicy>
    concept MemoryTaggingPolicy = requires(const TPolicy p, const void* ptr, const std::size_t size) {
        p.TagAllocation(ptr, size);
        p.TagDeallocation(ptr, size);
    };

    /**
     * @brief Disabled tagging policy; leaves memory untouched.
     */
    class NoMemoryTagging {
    public:
        NoMemoryTagging() = delete;
        auto operator==(const NoMemoryTagging &other) const -> bool = delete;

        static auto TagAllocation([[maybe_unused]] void *ptr, [[maybe_unused]] std::size_t size) -> void {}

        static auto TagDeallocation([[maybe_unused]] void *ptr,
                [[maybe_unused]] std::size_t size) -> void {}
    };
}
