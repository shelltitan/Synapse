#pragma once
#include <BitReader.hpp>
#include <Constant.hpp>
#include <cstdint>
#include <libassert/assert.hpp>
#include <SerialiseBit.hpp>

namespace Synapse::Serialise {
    /**
     * @class ReadStream
     * @brief High-level stream interface for reading bit-packed serialised data.
     *
     * This class wraps a low-level `BitReader` and provides a safe, range-checked
     * interface for deserialising various primitive types and bit-packed values.
     *
     * It is designed to prevent out-of-bounds reads and to ensure values remain
     * within specified ranges using `WouldReadPastEnd` checks.
     *
     * @see BitReader
     */
    class ReadStream {
    public:
        /**
         * @brief Constructs a ReadStream for reading bit-packed data from a buffer.
         *
         * @param buffer Pointer to the buffer containing bit-packed data. Must be aligned to a 32-bit boundary.
         * @param bytes The number of valid bytes in the buffer. It can be non-multiple of 4, but the actual buffer
         *              must be large enough to safely read past the end to the next 32-bit boundary.
         *
         * @note The buffer must remain valid for the lifetime of the stream.
         */
        ReadStream(const std::uint32_t *buffer, unsigned int bytes);

        /**
         * @brief Deserialises an integer value (signed or unsigned) from a bit-packed stream using a known range.
         *
         * Reads the minimum number of bits required to cover the range [TMin, TMax], and stores the result in `value`.
         * Supports both signed and unsigned integral types.
         *
         * @tparam T The target integer type (e.g. std::uint16_t, std::int32_t).
         * @tparam TMin The minimum value that `value` can take.
         * @tparam TMax The maximum value that `value` can take.
         * @param value Output variable where the deserialised value will be stored.
         * @return True if successful, false if reading would go past the buffer end.
         */
        template<typename T, std::size_t TMin, std::size_t TMax>
        [[nodiscard]] auto DeserialiseInteger(T &value) -> bool {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            static_assert(TMin < TMax, "Min must be less than Max");

            constexpr std::size_t bits = BitsRequired(TMin, TMax);
            static_assert(bits <= (sizeof(T) * Constants::bits_per_byte), "Bit width exceeds target type");

            if (m_reader.WouldReadPastEnd(bits)) [[unlikely]] {
                return false;
            }

            using UnsignedT = std::make_unsigned_t<T>;
            UnsignedT unsigned_value = m_reader.ReadBits(bits);
            value = static_cast<T>(unsigned_value + TMin);
            return true;
        }

        /**
         * @brief Deserialise a fixed number of bits into an unsigned integer.
         *
         * Reads up to 32 bits from the bitstream and stores the result in `value`.
         *
         * @tparam T The unsigned integer type to deserialise into. Must be `uint16_t` or `uint32_t`.
         * @param value Reference where the deserialised value will be stored.
         * @param bits Number of bits to read. Must be in the range [1, bit-width of T].
         * @return `true` if deserialisation succeeded, `false` if reading would exceed stream bounds.
         */
        template<typename T>
            requires(std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> || std::same_as<T, std::uint32_t>)
        [[nodiscard]] auto DeserialiseBits(T &value, unsigned int bits) -> bool {
            constexpr unsigned int max_bits = sizeof(T) * Constants::bits_per_byte;
            DEBUG_ASSERT(bits > 0U);
            DEBUG_ASSERT(bits <= max_bits);

            if (m_reader.WouldReadPastEnd(bits)) {
                return false;
            }

            value = static_cast<T>(m_reader.ReadBits(bits)); // returns uint32_t
            return true;
        }

        /**
         * @brief Deserialise an array of bytes (read from the stream).
         *
         * This function reads a specified number of bytes from the stream into
         * the given buffer. It first ensures proper byte alignment via `DeserialiseAlign()`.
         *
         * @param data   Pointer to the output buffer where the bytes will be stored.
         * @param bytes  Number of bytes to read from the stream.
         * @return `true` if the read succeeded, `false` if alignment failed or the stream lacks enough bits.
         */
        [[nodiscard]] auto DeserialiseBytes(std::byte *data, unsigned int bytes) -> bool;

        /**
         * @brief Deserialise byte alignment padding from the stream.
         *
         * This function checks if the stream is currently aligned to a byte boundary.
         * If not, it verifies that the padding bits up to the next byte boundary are all zero,
         * and skips them. It fails if the stream does not have enough bits remaining or if
         * non-zero padding is encountered.
         *
         * @return `true` if the alignment read and padding verification succeeded, `false` otherwise.
         */
        [[nodiscard]] auto DeserialiseAlign() -> bool;

        /**
         * @brief Deserialise a single bit and interpret it as a boolean value.
         *
         * Reads one bit from the stream and stores the result as a `bool`.
         *
         * @param value Reference to the boolean where the result will be stored.
         *              The value will be `true` if the bit is 1, and `false` if it is 0.
         *
         * @return `true` if the bit was successfully read; `false` if the stream did not have enough bits.
         */
        [[nodiscard]] auto DeserialiseBool(bool &value) -> bool;

        /**
         * @brief Deserialise a sequence number relative to a base value, using unsigned relative encoding.
         *
         * This function reads a relative unsigned integer from the stream, interpreted as the difference
         * between `sequence1` and `sequence2`. The result wraps modulo 2^16.
         *
         * @param sequence1  The base (reference) sequence number.
         * @param sequence2  Output: the reconstructed absolute sequence number.
         *
         * @return `true` if deserialisation succeeded, `false` otherwise.
         */
        [[nodiscard]] auto DeserialiseSequenceRelative(uint16_t sequence1, uint16_t &sequence2) -> bool;

        /**
         * @brief Deserialises an unsigned integer relative to a previous value using variable-width encoding.
         *
         * This method uses a sequence of 1-bit flags to determine how many bits are used to encode
         * the delta from the previous value. It prioritizes encoding small differences efficiently.
         * If none of the compact flags match, it falls back to a full-width read. The method is meant
         * for encoding sequences that wrap around.
         *
         * @tparam T An unsigned integer type (e.g., std::uint16_t or std::uint32_t).
         * @param previous The previous value in the sequence.
         * @param current Output variable where the decoded value is stored.
         * @return True if deserialisation succeeded, false otherwise.
         */
        template<typename T>
        [[nodiscard]] auto DeserialiseUnsignedIntegerRelative(T previous, T &current) -> bool {
            bool flag = false;

            if (!DeserialiseBool(flag)) [[unlikely]] {
                return false;
            }
            if (flag) {
                current = previous + 1U;
                return true;
            }

            if (!DeserialiseBool(flag)) [[unlikely]] {
                return false;
            }
            if (flag) {
                T difference{ 0U };
                if (!DeserialiseInteger<T, 2U, 5U>(difference)) [[unlikely]] {
                    return false;
                }
                current = previous + difference;
                return true;
            }

            if (!DeserialiseBool(flag)) [[unlikely]] {
                return false;
            }
            if (flag) {
                T difference{ 0U };
                if (!DeserialiseInteger<T, 6U, 21U>(difference)) [[unlikely]] {
                    return false;
                }
                current = previous + difference;
                return true;
            }

            if (!DeserialiseBool(flag)) [[unlikely]] {
                return false;
            }
            if (flag) {
                T difference{ 0U };
                if (!DeserialiseInteger<T, 22U, 277U>(difference)) [[unlikely]] {
                    return false;
                }
                current = previous + difference;
                return true;
            }

            if (!DeserialiseBool(flag)) [[unlikely]] {
                return false;
            }
            if (flag) {
                T difference{ 0U };
                if (!DeserialiseInteger<T, 278U, 4373U>(difference)) [[unlikely]] {
                    return false;
                }
                current = previous + difference;
                return true;
            }

            if constexpr (std::is_same_v<T, std::uint32_t>) {
                if (!DeserialiseBool(flag)) [[unlikely]] {
                    return false;
                }
                if (flag) {
                    T difference{ 0U };
                    if (!DeserialiseInteger<T, 4374U, 69909U>(difference)) [[unlikely]] {
                        return false;
                    }
                    current = previous + difference;
                    return true;
                }

                return DeserialiseInteger<T, 0U, std::numeric_limits<std::uint32_t>::max()>(current);
            }
            if constexpr (std::is_same_v<T, std::uint16_t>) {
                return DeserialiseInteger<T, 0U, std::numeric_limits<std::uint16_t>::max()>(current);
            }

            // Fallback only if T is not matched by any specialization (should never hit).
            return false;
        }

        /**
         * @brief Computes the number of padding bits needed to reach the next byte boundary.
         *
         * This function returns how many zero bits would need to be skipped
         * to align the stream to the next full byte, assuming an alignment were performed now.
         *
         * @return Number of bits in the range [0, 7] required to achieve byte alignment.
         */
        [[nodiscard]] auto GetAlignBits() const -> unsigned int;

        /**
         * @brief Returns the number of bits that have been read from the stream so far.
         *
         * This includes all bits processed since the beginning of the stream, including those
         * consumed by deserialisation and alignment operations.
         *
         * @return The total number of bits read from the stream.
         */
        [[nodiscard]] auto GetBitsProcessed() const -> unsigned int;

        /**
         * @brief Returns the number of bytes read from the stream so far.
         *
         * This is computed as the total number of bits read, rounded up to the next full byte.
         * It reflects how many bytes of the underlying buffer have been logically consumed.
         *
         * @return Number of bytes processed, in the range [0, ceil(bits_read / 8)].
         */
        [[nodiscard]] auto GetBytesProcessed() const -> unsigned int;

    private:
        BitReader m_reader; // The bit reader used for all bitpacked read operations.
    };
}
