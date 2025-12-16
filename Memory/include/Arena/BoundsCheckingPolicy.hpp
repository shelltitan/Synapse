#pragma once
#include <cstddef>
#include <concepts>

namespace Synapse::Memory::Arena {
        /*
         * Useful for finding memory stomps
         * Options:
         * No bounds checking
        */
    /// \todo Check all magic values at every allocation and deallocation
    /// \todo Check for magic value at freeing, if overwritten then we have memory corruption
    /**
     * @brief Concept for policies that insert and validate guard regions.
     */
    template <typename TPolicy>
    concept BoundsCheckingPolicy = requires(const TPolicy p) {
        { TPolicy::SIZE_FRONT } -> std::same_as<std::size_t>;
        { TPolicy::SIZE_BACK } -> std::same_as<std::size_t>;
        p.GuardFront();
        p.GuardBack();
        p.CheckFront();
        p.CheckBack();
    };

    /**
     * @brief No-op bounds checking policy.
     */
    class NoBoundsChecking {
    public:
        NoBoundsChecking() = delete;
        auto operator==(const NoBoundsChecking& other) const -> bool = delete;

        static constexpr std::size_t SIZE_FRONT{ 0 };
        static constexpr std::size_t SIZE_BACK{ 0 };

        static auto GuardFront([[maybe_unused]] void* ptr) -> void {}
        static auto GuardBack([[maybe_unused]] void* ptr) -> void {}

        static auto CheckFront([[maybe_unused]] const void* ptr) -> void {}
        static auto CheckBack([[maybe_unused]] const void* ptr) -> void {}
    };
}
