#pragma once
#include <Constant.hpp>
#include <bit>
#include <cstdint>
#include <limits>

namespace Synapse::Serialise {

    /**
     * @brief Calculates the number of bits required to serialise an integer in the range [min, max].
     *
     * Determines the minimum number of bits needed to represent any value in the inclusive range [min, max].
     * If min >= max, zero bits are required.
     *
     * @param min The minimum value in the range.
     * @param max The maximum value in the range.
     * @return The number of bits required to represent values in [min, max].
     */
    inline constexpr auto BitsRequired(const std::uint64_t min, const std::uint64_t max) -> unsigned int {
        if (min >= max) {
            return 0U;
        }
        return Constants::bits_per_uint64_t - static_cast<unsigned int>(std::countl_zero(max - min));
    }

    /**
     * @brief Converts an integer value from host (native) byte order to network byte order.
     *
     * Network byte order in this serialisation system is defined to be little-endian, regardless of platform.
     * This function ensures consistent binary representation across architectures.
     *
     * @tparam T The integer type (must be one of: uint64_t, uint32_t, uint16_t).
     * @param value The input value in host byte order.
     * @return The value converted to network byte order. On little-endian platforms, this is a no-op.
     *         On big-endian systems, the value is byte-swapped.
     *
     * @see serialise::bswap
     */
    template<typename T>
    constexpr auto HostToNetwork(T value) -> T {
        static_assert(std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t> ||
                              std::is_same_v<T, std::uint64_t>,
                "NetworkToHost only supports uint16_t, uint32_t, or uint64_t.");
        if constexpr (std::endian::native == std::endian::big) {
            return std::byteswap(value);
        } else if constexpr (std::endian::native == std::endian::little) {
            return value;
        }
    }

    /**
     * @brief Converts an integer value from network byte order to host (native) byte order.
     *
     * Network byte order in this serialisation system is defined to be little-endian, regardless of platform.
     * This function ensures consistent interpretation of binary data across architectures.
     *
     * @tparam T The integer type (must be one of: uint64_t, uint32_t, uint16_t).
     * @param value The input value in network (little-endian) byte order.
     * @return The value converted to host byte order. On little-endian systems, this is a no-op.
     *         On big-endian systems, the value is byte-swapped.
     *
     * @see serialise::bswap
     */
    template<typename T>
    constexpr auto NetworkToHost(T value) -> T {
        static_assert(std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t> ||
                              std::is_same_v<T, std::uint64_t>,
                "NetworkToHost only supports uint16_t, uint32_t, or uint64_t.");
        if constexpr (std::endian::native == std::endian::big) {
            return std::byteswap(value);
        } else if constexpr (std::endian::native == std::endian::little) {
            return value;
        }
    }

    /**
     * @brief Converts a signed 32-bit integer to an unsigned integer using zig-zag encoding.
     *
     * Zig-zag encoding maps signed integers to unsigned integers such that small
     * absolute values (both positive and negative) have small encoded values:
     *     0 → 0, -1 → 1, +1 → 2, -2 → 3, +2 → 4, ...
     *
     * This is useful in variable-length encoding schemes where smaller numbers take fewer bits.
     *
     * @param n The signed integer input.
     * @return The zig-zag encoded unsigned integer.
     *
     * @see ZigZagDecodeUnsignedToSigned
     */
    template<typename T>
    constexpr auto ZigZagEncodeSignedToUnsigned(T n) -> std::make_unsigned_t<T> {
        static_assert(std::is_signed_v<T>, "ZigZagEncodeSignedToUnsigned requires a signed integer type");
        using U = std::make_unsigned_t<T>;
        constexpr unsigned int shift = (sizeof(T) * Constants::bits_per_byte) - 1U;
        return (static_cast<U>(n) << 1) ^ static_cast<U>(n >> shift);
    }

    /**
     * @brief Converts an unsigned integer to a signed integer using zig-zag decoding.
     *
     * Zig-zag decoding undoes the transformation applied by zig-zag encoding:
     *     0 → 0, 1 → -1, 2 → +1, 3 → -2, 4 → +2, ...
     *
     * This is used to efficiently encode signed integers as small unsigned values.
     *
     * @tparam T An unsigned integer type (e.g., std::uint32_t).
     * @param n The zig-zag encoded unsigned integer.
     * @return The decoded signed integer.
     *
     * @see ZigZagEncodeSignedToUnsigned
     */
    template<typename T>
    constexpr auto ZigZagDecodeUnsignedToSigned(T n) -> std::make_signed_t<T> {
        static_assert(std::is_unsigned_v<T>, "ZigZagDecodeUnsignedToSigned requires a unsigned signed integer type");
        using SignedT = std::make_signed_t<T>;
        return static_cast<SignedT>(n >> 1U) ^ -static_cast<SignedT>(n & 1U);
    }

    /**
     * @brief Computes the number of bits required to encode a 16-bit sequence number relative to a previous value.
     *
     * This uses a tiered encoding scheme: small deltas use fewer bits with prefix flags,
     * and large deltas fall back to full 32-bit representation.
     *
     * @param first_sequence The base sequence number.
     * @param second_sequence The subsequent sequence number to encode relative to the first.
     * @return The total number of bits required for encoding.
     */
    inline constexpr auto GetRelativeSequenceEncodingBits(const std::uint16_t first_sequence, const std::uint16_t second_sequence)
            -> unsigned int {
        constexpr std::uint32_t wrap_around =
                static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;
        const std::uint32_t a = first_sequence;
        const std::uint32_t b = second_sequence + ((first_sequence >= second_sequence) ? wrap_around : 0U);

        if (const std::uint32_t difference = b - a; difference == 1U) {
            return 1U;
        } else {
            if (difference < 6U) {
                return 2U + BitsRequired(2U, 5U);
            }
            if (difference < 22U) {
                return 3U + BitsRequired(6U, 21U);
            }
            if (difference < 278U) {
                return 4U + BitsRequired(22U, 277U);
            }
            if (difference < 4374U) {
                return 5U + BitsRequired(278U, 4373U);
            }
            if (difference < 69910U) {
                return 6U + BitsRequired(4374U, 69909U);
            }
            return Constants::bits_per_uint32_t;
        }
    }
}
