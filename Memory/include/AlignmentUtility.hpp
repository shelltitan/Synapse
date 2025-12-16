#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <type_traits>
#include <libassert/assert.hpp>


namespace Synapse::Memory::Utility {
    /**
     * @brief Checks if a given size is aligned to the specified alignment.
     *
     * Determines whether `size` is a multiple of `alignment`, which implies
     * that it satisfies the specified alignment constraint. This is commonly
     * used to verify size or offset alignment in memory or buffer management.
     *
     * @param size       The value to check for alignment (typically a size or offset).
     * @param alignment  The alignment in bytes. Must be a power of two.
     *
     * @return `true` if `size` is a multiple of `alignment`, `false` otherwise.
     *
     * @note This function is `constexpr` and `noexcept`, allowing for compile-time
     * evaluation and safe use in noexcept contexts.
     *
     * @warning The function asserts at runtime (in debug mode) if `alignment` is not
     * a power of two.
     */
    [[nodiscard]] constexpr auto IsSizeAligned(const std::size_t size, const std::size_t alignment) noexcept
            -> bool {
        DEBUG_ASSERT(std::has_single_bit(alignment), "Invalid alignment. Must be power of two.");
        return (size & (alignment - 1U)) == 0U;
    }

    /**
     * @brief Rounds up a value to the next multiple of the specified alignment.
     *
     * Computes the smallest value greater than or equal to `val` that is a multiple
     * of `alignment`. This is typically used to ensure that buffer sizes or memory
     * offsets meet alignment requirements.
     *
     * @param val        The original size or value to align.
     * @param alignment  The alignment in bytes. Must be a power of two.
     *
     * @return The aligned size, which is the next multiple of `alignment` greater
     *         than or equal to `val`.
     *
     * @note This function is `constexpr` and `noexcept`, suitable for compile-time
     * evaluation and safe for noexcept contexts.
     *
     * @warning The function asserts at runtime (in debug mode) if `alignment` is not
     * a power of two.
     */
    [[nodiscard]] constexpr auto AlignSize(const std::size_t val, const std::size_t alignment) noexcept
            -> std::size_t {
        DEBUG_ASSERT(std::has_single_bit(alignment), "Invalid alignment. Must be power of two.");
        return (val + (alignment - 1U)) & ~(alignment - 1U);
    }

    /**
     * @brief Checks if a given pointer address is aligned to the specified byte boundary.
     *
     * This function verifies whether the address pointed to by `pointer` is aligned to
     * the provided `alignment` value. The alignment must be a power of two.
     *
     * @param pointer    Pointer to the memory address to check. Must not be nullptr.
     * @param alignment  Alignment in bytes to check against. Must be a power of two.
     *
     * @return `true` if the address is aligned to the specified alignment, `false` otherwise.
     *
     * @note This function is `constexpr` and `noexcept`, making it suitable for compile-time
     * evaluation and safe to use in noexcept contexts.
     *
     * @warning The function asserts at runtime (in debug mode) if `pointer` is null or
     * if `alignment` is not a power of two.
     */
    [[nodiscard]] constexpr auto IsAddressAligned(const void *pointer, const std::size_t alignment) noexcept
            -> bool {
        DEBUG_ASSERT(pointer != nullptr, "Invalid pointer");
        DEBUG_ASSERT(std::has_single_bit(alignment), "Invalid alignment. Must be power of two.");
        return static_cast<std::size_t>(std::bit_cast<uintptr_t>(pointer) & (alignment - 1U)) == 0U;
    }

    /**
     * @brief Aligns a pointer to the specified power-of-two alignment.
     *
     * This function takes a raw pointer and returns the nearest aligned address
     * that is greater than or equal to the original address. It is useful for aligning
     * memory blocks to meet hardware or API alignment requirements.
     *
     * @param pointer The original unaligned pointer.
     * @param alignment The desired alignment in bytes. Must be a power of two.
     * @return The aligned pointer, cast to `std::byte*`.
     *
     * @note The function assumes that `pointer` is not null and that `alignment` is a valid power of two.
     *       These conditions are enforced via `DEBUG_ASSERT`.
     */
    [[nodiscard]] constexpr auto AlignAddress(const std::byte *pointer, const std::size_t alignment) noexcept
            -> std::byte * {
        DEBUG_ASSERT(pointer != nullptr, "Invalid pointer");
        DEBUG_ASSERT(std::has_single_bit(alignment), "Invalid alignment. Must be power of two.");
        return std::bit_cast<std::byte *>((std::bit_cast<uintptr_t>(pointer) + (alignment - 1U)) & ~(alignment - 1U));
    }
}