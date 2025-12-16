#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>

namespace Synapse::Serialise {
    /**
     * @class BitWriter
     * @brief Serialises unsigned and signed integer values into a bit-packed buffer.
     *
     * Bits are written into a 64-bit scratch register from right to left. When the
     * scratch register accumulates 32 or more bits, the lower 32 bits are flushed
     * to the output buffer as a 32-bit unsigned integer, and the scratch is shifted
     * right by 32 bits.
     *
     * The output bit stream is written in little-endian order, which this library
     * treats as network byte order.
     *
     * The target buffer must be 4-byte aligned and its size must be a multiple of 4 bytes.
     *
     * @see BitReader
     */
    class BitWriter {
    public:
        /**
         * @brief Constructs a BitWriter for writing bit-packed data to the given buffer.
         *
         * Initializes the writer to operate on a buffer of the specified size. The buffer
         * must be 4-byte aligned and its size must be a multiple of 4 bytes, as data is
         * written in 32-bit chunks.
         *
         * @param data Pointer to the output buffer to fill with bit-packed data.
         * @param bytes Size of the buffer in bytes. Must be a multiple of 4.
         */
        BitWriter(std::uint32_t *data, unsigned int bytes);

        /**
         * @brief Writes a fixed number of bits to the output buffer.
         *
         * Writes the least-significant @p bits of @p value to the internal scratch buffer.
         * Bits are written in a tightly packed form without any padding to byte boundaries.
         * This allows values like booleans (1 bit) or bounded integers (e.g., 5 bits for values in [0,31])
         * to be efficiently encoded.
         *
         * IMPORTANT: You must call `BitWriter::FlushBits()` after all writing is complete
         * to ensure that any remaining bits in the scratch buffer are flushed to memory.
         * Failure to do so may result in the final 32-bit unsigned integer of output being omitted.
         *
         * The function will assert in debug builds if:
         * - @p bits is not in [1, 32]
         * - @p value exceeds the maximum representable value in @p bits
         * - The write operation would exceed the bounds of the output buffer
         *
         * @param value The value to write. Must be in the range [0, (1 << bits) - 1].
         * @param bits  The number of bits to write, in the range [1, 32].
         *
         * @see BitReader::ReadBits
         * @see BitWriter::FlushBits
         */
        auto WriteBits(std::uint32_t value, unsigned int bits) -> void;

        /**
         * @brief Pads zero bits to align the bit stream to the next byte boundary.
         *
         * If the current bit index is not already a multiple of 8, this function writes
         * zero bits to advance the bit stream to the next byte boundary. This is useful
         * when writing data types that require byte alignment, such as raw byte arrays or strings.
         *
         * IMPORTANT: If the bit index is already byte-aligned, this function writes nothing.
         *
         * @see BitReader::SkipToByteBoundaryAndVerifyZeroPadding
         */
        auto WriteZeroPaddingToAlignByteBoundary() -> void;

        /**
         * @brief Writes a block of raw bytes to the bit stream.
         *
         * This function efficiently writes @p bytes of data from the given @p data pointer
         * into the bit stream. It performs fast memory copying for aligned 32-bit chunks and
         * falls back to per-byte `WriteBits(value, 8)` for unaligned head and tail regions.
         *
         * Use this method when inserting larger byte arrays (e.g., serialised structs or blobs),
         * as it avoids bit-by-bit packing where possible.
         *
         * The current bit position must be byte-aligned; this function will assert otherwise.
         *
         * @param data Pointer to the byte array to write.
         * @param bytes Number of bytes to write into the stream.
         *
         * @see BitReader::ReadBytes
         */
        auto WriteBytes(const std::byte *data, unsigned int bytes) -> void;

        /**
         * @brief Flushes any remaining bits in the scratch buffer to memory.
         *
         * This function must be called after all `WriteBits` operations are complete
         * to ensure the final 32-bit unsigned integer is written to the output buffer.
         * If the scratch buffer contains less than 32 bits, those bits are flushed as-is,
         * zero-padded in the upper bits.
         *
         * Failing to call this function will result in the final partial data being lost.
         *
         * @see BitWriter::WriteBits
         */
        auto FlushBits() -> void;

        /**
         * @brief Calculates how many padding bits would be written to reach the next byte boundary.
         *
         * This function determines how many zero bits would be needed to align the current
         * bit position to the next byte boundary, if an alignment operation were performed now.
         *
         * @return A value in the range [0, 7], where 0 means already byte-aligned and 7 is the worst case.
         */
        [[nodiscard]] auto GetAlignBits() const -> unsigned int;

        /**
         * @brief Returns the number of bits written to the bit-packed buffer so far.
         *
         * This reflects the current write position in bits, relative to the start of the buffer.
         *
         * @return The total number of bits written.
         */
        [[nodiscard]] auto GetBitsWritten() const -> unsigned int;

        /**
         * @brief Returns the number of bits still available for writing.
         *
         * This is the remaining capacity of the buffer in bits, based on the total
         * allocated size and the number of bits already written.
         *
         * For example, if the buffer holds 32 bits and 10 bits have been written,
         * this function will return 22.
         *
         * @return The number of bits available to write.
         */
        [[nodiscard]] auto GetBitsAvailable() const -> unsigned int;

        /**
         * @brief Returns a pointer to the underlying output buffer.
         *
         * This corresponds to the original buffer passed to the constructor and contains
         * the bit-packed data written so far. The pointer is returned as a `std::byte*`
         * for compatibility with byte-oriented operations (e.g., network transmission).
         *
         * @return Pointer to the output buffer containing the written data.
         */
        [[nodiscard]] auto GetData() const -> const std::byte *;

        /**
         * @brief Returns the number of bytes written to the buffer so far.
         *
         * This is the total size of the bit-packed data in bytes and represents the
         * exact number of bytes that should be transmitted or stored.
         *
         * Note: The returned size is not necessarily a multiple of 4, even though data is flushed
         * in 32-bit (4-byte) chunks. This is safe because bits are written in little-endian order,
         * and any partial final 32-bit unsigned integer is valid in memory as long as `FlushBits()` has been called.
         *
         * IMPORTANT: You must call `BitWriter::FlushBits()` before invoking this function to ensure
         * the final partial 32-bit unsigned integer is written to the buffer.
         *
         * @return The number of bytes of bit-packed data written.
         */
        [[nodiscard]] auto GetBytesWritten() const -> unsigned int;

        /**
         * @brief Returns a pointer to the output buffer as a byte array.
         *
         * Provides access to the underlying bit-packed buffer as a `const std::byte*`,
         * suitable for writing to sockets, files, or other byte-oriented interfaces.
         *
         * @return Pointer to the start of the output buffer in byte form.
         */
        [[nodiscard]] auto GetDataBytes() const -> const std::byte * { return std::bit_cast<std::byte *>(m_data); }

    private:
        /// Total number of 32-bit integers in the buffer (buffer size in bytes divided by 4).
        unsigned int m_number_of_32_bit_ints;

        /// Number of bits written so far.
        unsigned int m_bits_written{ 0U };

        /// Index of the next 32-bit integer to be written to in the output buffer.
        unsigned int m_32_bit_int_index{ 0U };

        /// Number of valid bits currently in the scratch register.
        /// When this is >= 32, the lower 32 bits are flushed to memory and the scratch is shifted right.
        unsigned int m_scratch_bits{ 0U };

        /// Scratch register that holds bits before they are flushed to memory.
        /// Bits are written right to left (least-significant first), and flushed in 32-bit chunks.
        std::uint64_t m_scratch{ 0U };

        /// Output buffer, interpreted as an array of 32-bit integers for efficient aligned writes.
        std::uint32_t *m_data;
    };
}
