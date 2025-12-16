#pragma once
#include <cstdint>

namespace Synapse::Maths {
    // Implement a "bit-twiddling hack" for finding the next power of 2 in either 32 bits or 64 bits in constexpr functions.

    template <class T>
    constexpr auto Decrement(T x) noexcept -> T {
        return x - 1U;
    }

    template <class T>
    constexpr auto Increment(T x) noexcept -> T {
        return x + 1U;
    }

    template <class T>
    constexpr auto OrEqual(T x, unsigned u) noexcept -> T {
        return x | x >> u;
    }

    template <class T, class... Args>
    constexpr auto OrEqual(T x, unsigned u, Args... rest) noexcept -> T {
        return OrEqual(OrEqual(x, u), rest...);
    }

    // Finding the next power of 2 in 32 bits in constexpr functions.
    constexpr auto RoundUpToPowerOf2(const std::uint32_t a) noexcept -> std::uint32_t {
        return Increment(OrEqual(Decrement(a), 1, 2, 4, 8, 16));
    }
    // Finding the next power of 2 in 64 bits in constexpr functions.
    constexpr auto RoundUpToPowerOf2(const std::uint64_t a) noexcept -> std::uint64_t {
        return Increment(OrEqual(Decrement(a), 1, 2, 4, 8, 16, 32));
    }
}
