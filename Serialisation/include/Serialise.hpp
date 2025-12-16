#pragma once
#include <array>
#include <bit>
#include <libassert/assert.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <Constant.hpp>

namespace Synapse::Serialise {

    /**
     * @brief Writes an integral value to a byte buffer in little-endian order.
     *
     * This function safely serialises any integral type (`uint8_t`, `int16_t`, `uint32_t`, etc.)
     * into a buffer of `std::byte*`, handling endianness at compile time using `if constexpr`.
     *
     * On little-endian systems, the value is written directly. On big-endian systems, the
     * value is byte-swapped before writing. Signed values are safely converted via unsigned
     * promotion before swapping.
     *
     * @tparam T The integral type to write (must satisfy `std::is_integral_v<T>`).
     * @param p A pointer to a pointer to the write position in the destination buffer.
     *          This will be advanced by `sizeof(T)` after writing.
     * @param value The integral value to write.
     *
     * @note Uses `std::bit_cast` and `std::copy_n` for safe memory operations.
     * @note This function assumes there is enough space in the buffer.
     *
     * @see std::bit_cast, std::byteswap, std::endian
     */
    template<typename T>
    inline auto WriteInteger(std::byte **p, T value) -> void {
        DEBUG_ASSERT(p != nullptr);
        DEBUG_ASSERT(*p != nullptr);
        static_assert(std::is_integral_v<T>, "WriteInteger only supports integral types");

        if constexpr (std::endian::native == std::endian::little) {
            // No byte swap needed
        } else if constexpr (std::is_unsigned_v<T>) {
            value = std::byteswap(value);
        } else {
            using UnsignedT = std::make_unsigned_t<T>;
            value = static_cast<T>(std::byteswap(static_cast<UnsignedT>(value)));
        }

        const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        (void)std::copy_n(bytes.begin(), sizeof(T), *p);
        *p += sizeof(T);
    }

    /**
     * @brief Writes a block of raw bytes into the destination buffer.
     *
     * Copies `num_bytes` from the source `byte_array` to the output buffer pointed to by `*p`,
     * and advances the pointer accordingly.
     *
     * @param p Pointer to a pointer to the destination buffer. Will be advanced by `num_bytes`.
     * @param byte_array Pointer to the source byte array to copy.
     * @param num_bytes Number of bytes to copy.
     *
     * @note This is functionally equivalent to `memcpy`, but uses `std::copy_n` for type safety.
     */
    inline auto WriteBytes(std::byte **p, const std::byte *byte_array, std::size_t num_bytes) -> void {
        DEBUG_ASSERT(p != nullptr);
        DEBUG_ASSERT(*p != nullptr);
        DEBUG_ASSERT(byte_array != nullptr);
        DEBUG_ASSERT(num_bytes > 0U);

        (void)std::copy_n(byte_array, num_bytes, *p);
        *p += num_bytes;
    }

    /**
     * @brief Reads an integer of type T from the buffer, handling endianness.
     *
     * This function reads `sizeof(T)` bytes from the buffer pointed to by `*p`,
     * interprets them as an integer of type `T`, and advances the pointer.
     *
     * @tparam T The integral type to read (e.g., uint16_t, int32_t).
     * @param p Pointer to a pointer to the byte buffer. Will be advanced by sizeof(T).
     * @return The integer value read from the buffer.
     */
    template<typename T>
    inline auto ReadInteger(const std::byte **p) -> T {
        DEBUG_ASSERT(p != nullptr);
        DEBUG_ASSERT(*p != nullptr);
        static_assert(std::is_integral_v<T>, "ReadInteger only supports integral types");

        std::array<std::byte, sizeof(T)> bytes;
        (void)std::copy_n(*p, sizeof(T), bytes.begin());
        *p += sizeof(T);

        T value = std::bit_cast<T>(bytes);

        if constexpr (std::endian::native == std::endian::big) {
            if constexpr (std::is_unsigned_v<T>) {
                value = std::byteswap(value);
            } else {
                using UnsignedT = std::make_unsigned_t<T>;
                value = static_cast<T>(std::byteswap(static_cast<UnsignedT>(value)));
            }
        }

        return value;
    }

    /**
     * @brief Reads a block of raw bytes from the source buffer.
     *
     * Copies `num_bytes` from the buffer pointed to by `*p` into the destination `byte_array`,
     * and advances the source pointer accordingly.
     *
     * @param p Pointer to a pointer to the source buffer. Will be advanced by `num_bytes`.
     * @param byte_array Pointer to the destination byte array.
     * @param num_bytes Number of bytes to read.
     */
    inline auto ReadBytes(const std::byte **p, std::byte *byte_array, std::size_t num_bytes) -> void {
        DEBUG_ASSERT(p != nullptr);
        DEBUG_ASSERT(*p != nullptr);
        DEBUG_ASSERT(byte_array != nullptr);
        DEBUG_ASSERT(num_bytes > 0U);
        (void)std::copy_n(*p, num_bytes, byte_array);
        *p += num_bytes;
    }

    /**
     * @brief Safely copies a C-style string into a fixed-size destination buffer.
     *
     * Copies characters from the source string into the destination buffer up to `dest_size - 1` characters,
     * ensuring the result is null-terminated. If the source string is longer than the destination buffer,
     * it will be truncated safely. This function avoids buffer overruns.
     *
     * @param dest Pointer to the destination buffer. Must not be null. Will be null-terminated.
     * @param source Pointer to the source null-terminated string. Must not be null.
     * @param dest_size Size of the destination buffer in bytes. Must be greater than 0.
     */
    inline auto CopyString(char *dest, const char *source, std::size_t dest_size) -> void {
        DEBUG_ASSERT(dest != nullptr);
        DEBUG_ASSERT(source != nullptr);
        DEBUG_ASSERT(dest_size > 0U);

        std::size_t i = 0U;
        for (; i < dest_size - 1U && source[i] != '\0'; ++i) {
            dest[i] = source[i];
        }
        dest[i] = '\0'; // Ensure null termination
    }

    /**
     * @brief Compile-time capable safe copy of a null-terminated C-string into a fixed-size buffer.
     *
     * This function copies characters from the source string into the destination array, ensuring null termination
     * and avoiding buffer overruns. It is `constexpr` and can be evaluated at compile time when all parameters are
     * known.
     *
     * @tparam N Size of the destination buffer.
     * @param dest Destination character array. Will always be null-terminated.
     * @param source Null-terminated input string.
     */
    template<std::size_t N>
    constexpr void CopyString(char (&dest)[N], const char *source) {
        static_assert(N > 0, "Destination size must be greater than 0");
        std::size_t i = 0;
        while (i < N - 1 && source[i] != '\0') {
            dest[i] = source[i];
            ++i;
        }
        dest[i] = '\0';
    }

    /**
     * @brief Calculates the minimum number of bytes required to encode a 64-bit sequence number.
     *
     * This function determines how many bytes are needed to represent a given 64-bit unsigned integer
     * without losing any non-zero bits. It is commonly used in variable-length encoding schemes to
     * minimise bandwidth or storage for small values.
     *
     * @param sequence The 64-bit unsigned sequence number to evaluate.
     * @return The number of bytes required to represent the sequence value. Guaranteed to be in [1, 8].
     *
     * @note A value of 0 always requires 1 byte.
     */
    inline constexpr auto SequenceNumberBytesRequired(const std::uint64_t sequence) -> unsigned int {
        if (sequence == 0U) {
            return 1U;
        }
        return (Constants::bits_per_uint64_t - static_cast<unsigned int>(std::countl_zero(sequence)) + 7U) / Constants::bits_per_byte;
    }
}
