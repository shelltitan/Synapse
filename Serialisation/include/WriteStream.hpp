#pragma once
#include <BitWriter.hpp>
#include <libassert/assert.hpp>
#include <Constant.hpp>
#include <SerialiseBit.hpp>

namespace Synapse::Serialise {
    /**
     * @class WriteStream
     * @brief High-level stream interface for writing bit-packed serialised data.
     *
     * This class wraps a low-level `BitWriter` and provides a safe, efficient interface
     * for serialising primitive types and bit-packed values into a compact binary format.
     *
     * It ensures alignment and enforces bit-level correctness when writing to the stream.
     * All writes are buffered and flushed in 32-bit units for performance and consistency.
     *
     * This class is the counterpart to `ReadStream` and is typically used alongside it
     * in serialisation routines that mirror read/write logic.
     *
     * @see BitWriter
     * @see ReadStream
     */
    class WriteStream {
    public:
        /**
         * @brief Constructs a WriteStream over a fixed-size memory buffer.
         *
         * Initializes the internal bit writer to write to the provided buffer. The buffer
         * must be non-null, 4-byte aligned, and its size must be a multiple of 4 bytes,
         * since all data is written in 32-bit units.
         *
         * @param buffer Pointer to the destination buffer. Must not be null and must be aligned to
         * `alignof(std::uint32_t)`.
         * @param bytes  Size of the buffer in bytes. Must be a multiple of 4.
         */
        WriteStream(std::uint32_t *buffer, unsigned int bytes);

        /**
         * @brief Serialises an integer value (signed or unsigned) to a bit-packed stream using a known range.
         *
         * Writes the minimum number of bits required to represent values in the range [TMin, TMax].
         * Supports both signed and unsigned integral types.
         *
         * @tparam T      The integer type to serialise (e.g. std::uint16_t, std::int32_t).
         * @tparam TMin   The minimum value that `value` can take.
         * @tparam TMax   The maximum value that `value` can take.
         * @param value   The integer value to serialise. Must be in [TMin, TMax].
         * @return Always returns true. Range checking is performed only via debug assertions.
         */
        template<typename T, std::size_t TMin, std::size_t TMax>
        [[nodiscard]] auto SerialiseInteger(T value) -> bool {
            static_assert(std::is_integral_v<T>, "T must be an integral type");
            static_assert(TMin < TMax, "TMin must be less than TMax");

            constexpr std::size_t bits = BitsRequired(TMin, TMax);
            static_assert(bits <= (sizeof(T) * Constants::bits_per_byte), "Bit width exceeds type capacity");

            DEBUG_ASSERT(value >= static_cast<T>(TMin));
            DEBUG_ASSERT(value <= static_cast<T>(TMax));

            using UnsignedT = std::make_unsigned_t<T>;
            const auto unsigned_value = static_cast<UnsignedT>(value - static_cast<T>(TMin));
            m_writer.WriteBits(static_cast<std::uint32_t>(unsigned_value), bits);
            return true;
        }

        /**
         * @brief Serialises a fixed number of bits from an unsigned integer.
         *
         * Writes the lower `bits` bits of `value` into the stream. The value must be in the range [0, (1 << bits) - 1].
         * This function assumes all range checking is performed via debug assertions.
         *
         * @param value The unsigned integer value to serialise.
         * @param bits  The number of bits to write. Must be in the range [1, 32].
         * @return Always returns `true`. Range validation is enforced only in debug builds.
         */
        [[nodiscard]] auto SerialiseBits(std::uint32_t value, unsigned int bits) -> bool;
        
        /**
         * @brief Serialises an array of bytes into the stream.
         *
         * Ensures byte alignment before writing and then writes `bytes` bytes from the given array.
         * Assumes all validation is done via debug assertions.
         *
         * @param data   Pointer to the array of bytes to write. Must not be null.
         * @param bytes  Number of bytes to write. May be zero.
         * @return Always returns `true`. Range and alignment checking is only enforced in debug builds.
         */
        [[nodiscard]] auto SerialiseBytes(const std::byte *data, unsigned int bytes) -> bool;

        /**
         * @brief Serialises alignment padding to reach the next byte boundary.
         *
         * Writes zero bits to pad the stream to the next byte boundary, if necessary.
         * This ensures subsequent byte-aligned writes (e.g. `SerialiseBytes`) begin on a clean boundary.
         * All correctness checks are enforced via debug assertions.
         *
         * @return Always returns `true`. No runtime failure conditions are expected in release builds.
         */
        auto SerialiseAlign() -> void;

        /**
         * @brief Returns the number of zero bits that would be written to align to the next byte boundary.
         *
         * This method computes how many padding bits would be required if `SerialiseAlign()` were called now.
         * The result is always in the range [0, 7], where 0 means the stream is already byte-aligned.
         *
         * @return Number of zero bits needed to achieve byte alignment.
         */
        [[nodiscard]] auto GetAlignBits() const -> unsigned int;

        /**
         * @brief Flushes any unwritten bits to the output buffer.
         *
         * Ensures that all bits written to the stream are flushed to memory, including any partial 32-bit words.
         * This must be called after writing is complete and before accessing the output via `WriteStream::GetData()`,
         * otherwise the last word may be incomplete or missing.
         *
         * @note Failing to flush may result in truncated output.
         * @see BitWriter::FlushBits
         */
        auto Flush() -> void;

        /**
         * @brief Returns a pointer to the data written by the stream.
         *
         * This provides access to the internal output buffer containing the bit-packed data.
         * You must call `WriteStream::Flush()` before calling this function to ensure all bits
         * have been written to memory. Failing to do so may result in truncated or incomplete data.
         *
         * @return Pointer to the start of the written data buffer.
         *
         * @see WriteStream::Flush
         */
        [[nodiscard]] auto GetData() const -> const std::byte *;

        /**
         * @brief Returns the number of bytes written to the stream so far.
         *
         * This reflects the total number of bytes currently stored in the output buffer.
         * It is effectively the size of the serialised packet and should be used when copying
         * or transmitting the buffer returned by `GetData()`.
         *
         * @return Number of bytes written to the stream.
         *
         * @see WriteStream::GetData
         */
        [[nodiscard]] auto GetBytesProcessed() const -> unsigned int;

        /**
         * @brief Returns the number of bits written to the stream so far.
         *
         * This provides the exact number of bits written, including partial bytes.
         * Useful for diagnostics, debugging, or estimating compression efficiency.
         *
         * @return Total number of bits written to the stream.
         *
         * @see WriteStream::GetBytesProcessed
         */
        [[nodiscard]] auto GetBitsProcessed() const -> unsigned int;

    private:
        BitWriter m_writer; // The bit writer used for all bitpacked write operations.
    };
}