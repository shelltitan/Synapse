#pragma once

#include <cstddef>
#include <cstdint>

namespace Synapse::Serialise {
    /**
     * @class BitReader
     * @brief Reads bit-packed integer values from a 4-byte aligned buffer.
     *
     * This class interprets the input as an unattributed bit-packed binary stream.
     * It requires that the user performs bit reads in the exact same sequence as the
     * bits were originally written. There is no metadata or structure in the stream.
     *
     * @details Internally, the class loads 32-bit unsigned integers from memory into
     * the high bits of a 64-bit unsigned integer scratch value as needed.
     * Bits are read from the scratch value starting at the least significant bits (right side),
     * and the scratch is shifted right by the number of bits read after each operation.
     */
    class BitReader {
    public:
        /**
         * @brief Constructs a BitReader to read from a bit-packed data buffer.
         *
         * Supports buffer sizes that are not multiples of four, which is common when
         * receiving packets over the network. However, the actual memory allocated for
         * the buffer must be rounded up to at least the next 4-byte boundary, as the
         * BitReader reads 32-bit integers from memory, not individual bytes.
         *
         * @param data Pointer to the bit-packed data to read.
         * @param bytes The number of valid bytes of bit-packed data in the buffer.
         *
         * @see BitWriter
         */
        BitReader(const std::uint32_t *data, unsigned int bytes);

        /**
         * @brief Checks whether reading the specified number of bits would exceed the buffer.
         *
         * Determines if reading @p bits from the current position would go past the end
         * of the bit-packed data buffer.
         *
         * @param bits The number of bits to check for a potential read.
         * @return True if reading the specified number of bits would exceed the buffer;
         *         otherwise, false.
         */
        [[nodiscard]] auto WouldReadPastEnd(unsigned int bits) const -> bool;

        /**
         * @brief Reads a fixed number of bits from the bit-packed buffer.
         *
         * This function extracts @p bits from the buffer and returns them as a 32-bit unsigned integer.
         * In debug builds, it asserts if the read goes past the end of the buffer or if the number
         * of bits requested is out of the valid range.
         *
         * In production, higher-level code (e.g., `ReadStream`) is responsible for ensuring this function
         * is never called in such a case.
         *
         * @param bits The number of bits to read. Must be in the range [1, 32].
         * @return A 32-bit unsigned integer containing the read bits. The result is in the range [0, (1 << bits) - 1].
         *
         * @see BitReader::WouldReadPastEnd
         * @see BitWriter::WriteBits
         */
        [[nodiscard]] auto ReadBits(unsigned int bits) -> std::uint32_t;

        /**
         * @brief Skips to the next byte boundary and verifies that the skipped padding bits are zero.
         *
         * Call this to correspond to a prior `WriteAlign` call during serialisation. This ensures that
         * the read position is aligned to the next byte boundary. As a safety measure, any padding bits
         * between the current bit position and the next byte boundary are checked to be zero.
         *
         * If non-zero bits are found in the padding, the function returns false, which typically signals
         * a serialisation mismatch or data corruption. This check helps catch bugs or tampering early.
         *
         * @return True if padding bits were zero and alignment succeeded; false otherwise.
         *
         * @see BitWriter::WriteAlign
         */
        [[nodiscard]] auto SkipToByteBoundaryAndVerifyZeroPadding() -> bool;

        /**
         * @brief Reads a sequence of raw bytes from the bit-packed data stream.
         *
         * Reads exactly @p bytes from the stream into the destination buffer @p data. The read position
         * must be byte-aligned when this function is called; if not, it will assert in debug builds.
         *
         * The read is split into three parts:
         * - Head: a few unaligned bytes to reach the next 32-bit boundary (if necessary),
         * - Middle: a bulk copy of aligned 32-bit chunks using `std::copy_n`,
         * - Tail: a few remaining bytes (less than 4) at the end.
         *
         * This mirrors the behaviour of `BitWriter::WriteBytes`, and relies on the user maintaining
         * strict alignment symmetry between read and write sides.
         *
         * @param data Destination buffer to copy bytes into.
         * @param bytes Number of bytes to read from the stream.
         *
         * @see BitWriter::WriteBytes
         */
        auto ReadBytes(std::byte *data, unsigned int bytes) -> void;

        /**
         * @brief Calculates how many bits would need to be read to align to the next byte boundary.
         *
         * This function determines the number of padding bits required to reach the next
         * byte boundary, assuming an alignment operation were to occur at the current bit position.
         *
         * @return A value in the range [0, 7], where 0 indicates that the reader is already
         *         byte-aligned, and 7 is the maximum number of bits that might be skipped.
         */
        [[nodiscard]] auto GetAlignBits() const -> unsigned int;

        /**
         * @brief Returns the number of bits read from the bit-packed buffer so far.
         *
         * This represents the current read position in bits, relative to the start of the stream.
         *
         * @return The total number of bits that have been read.
         */
        [[nodiscard]] auto GetBitsRead() const -> unsigned int;

        /**
         * @brief Returns the number of bits remaining to be read from the bit-packed buffer.
         *
         * This represents the number of unread bits between the current read position and
         * the end of the buffer. For example, if the buffer is 4 bytes (32 bits) and
         * 10 bits have already been read, 22 bits remain.
         *
         * @return The number of bits still available to read.
         */
        [[nodiscard]] auto GetBitsRemaining() const -> unsigned int;

    private:
        /// Number of valid (logical) bytes in the buffer. This is the unrounded, actual input size.
        unsigned int m_number_of_bytes;

#ifdef _DEBUG
        /// Number of 32-bit words to read from memory (rounded up from m_number_of_bytes for safe access).
        unsigned int m_number_of_32_bit_ints;
#endif
        /// Number of bits read so far from the buffer.
        unsigned int m_bits_read{ 0U };

        /// Number of valid bits currently in the scratch buffer.
        /// If a read request exceeds this, another 32-bit word must be loaded from memory.
        unsigned int m_scratch_bits{
            0U
        };

        /// Index of the next 32-bit integer to load from the underlying memory buffer.
        unsigned int m_32_bit_int_index{ 0U };

        /// Scratch buffer used for bit reads. New data is loaded into the high bits (left),
        /// and bits are consumed from the low bits (right).
        std::uint64_t m_scratch{ 0U };

        /// Pointer to the 32-bit bit-packed data buffer. This is externally allocated and must
        /// be 4-byte aligned and zero-padded if necessary.
        const std::uint32_t *m_bitpacked_data;
    };
}
