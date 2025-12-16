#include <AlignmentUtility.hpp>
#include <BitWriter.hpp>
#include <Constant.hpp>
#include <SerialiseBit.hpp>
#include <algorithm>
#include <cstring>
#include <libassert/assert.hpp>

namespace Synapse::Serialise {
    BitWriter::BitWriter(std::uint32_t *data, const unsigned int bytes) :
        m_number_of_32_bit_ints(bytes / static_cast<unsigned int>(sizeof(std::uint32_t))), m_data(data) {
        DEBUG_ASSERT(data != nullptr);
        DEBUG_ASSERT(Utility::IsAddressAligned(data, alignof(std::uint32_t)));
        DEBUG_ASSERT((bytes % sizeof(std::uint32_t)) == 0U);
        DEBUG_ASSERT(bytes > 0U);
    }

    auto BitWriter::WriteBits(std::uint32_t value, const unsigned int bits) -> void {
        DEBUG_ASSERT(bits > 0U);
        DEBUG_ASSERT(bits <= Constants::bits_per_uint32_t);
        DEBUG_ASSERT((m_bits_written + bits) <= (m_number_of_32_bit_ints * Constants::bits_per_uint32_t));
        DEBUG_ASSERT(static_cast<std::uint64_t>(value) <= ((1ULL << bits) - 1ULL));

        m_scratch |= static_cast<std::uint64_t>(value) << m_scratch_bits;

        m_scratch_bits += bits;

        if (m_scratch_bits >= Constants::bits_per_uint32_t) {
            DEBUG_ASSERT(m_32_bit_int_index < m_number_of_32_bit_ints);
            m_data[m_32_bit_int_index] =
                    HostToNetwork(static_cast<std::uint32_t>(m_scratch & std::numeric_limits<std::uint32_t>::max()));
            m_scratch >>= Constants::bits_per_uint32_t;
            m_scratch_bits -= Constants::bits_per_uint32_t;
            ++m_32_bit_int_index;
        }

        m_bits_written += bits;
    }

    auto BitWriter::WriteZeroPaddingToAlignByteBoundary() -> void {
        const unsigned int remainder_bits = m_bits_written % Constants::bits_per_byte;

        if (remainder_bits != 0U) {
            WriteBits(0U, Constants::bits_per_byte - remainder_bits);
            DEBUG_ASSERT((m_bits_written % Constants::bits_per_byte) == 0U);
        }
    }

    auto BitWriter::WriteBytes(const std::byte *data, const unsigned int bytes) -> void {
        DEBUG_ASSERT(bytes > 0U);
        DEBUG_ASSERT((m_bits_written + (bytes * Constants::bits_per_byte)) <= (m_number_of_32_bit_ints * Constants::bits_per_uint32_t));
        DEBUG_ASSERT((m_bits_written % Constants::bits_per_byte) == 0U);

        unsigned int head_bytes =
                (static_cast<unsigned int>(sizeof(std::uint32_t)) - (m_bits_written % Constants::bits_per_uint32_t) / Constants::bits_per_byte) %
                static_cast<unsigned int>(sizeof(std::uint32_t));
        if (head_bytes > bytes) {
            head_bytes = bytes;
        }
        for (unsigned int i = 0U; i < head_bytes; ++i) {
            WriteBits(std::bit_cast<std::uint8_t>(data[i]), Constants::bits_per_byte);
        }
        if (head_bytes == bytes) {
            return;
        }

        const unsigned int number_of_32_bit_integers = (bytes - head_bytes) / static_cast<unsigned int>(sizeof(std::uint32_t));
        if (number_of_32_bit_integers > 0U) {
            DEBUG_ASSERT((m_bits_written % Constants::bits_per_uint32_t) == 0U);
            (void) std::copy_n(data + head_bytes, number_of_32_bit_integers * sizeof(std::uint32_t),
                    std::bit_cast<std::byte *>(&m_data[m_32_bit_int_index]));
            m_bits_written += number_of_32_bit_integers * Constants::bits_per_uint32_t;
            m_32_bit_int_index += number_of_32_bit_integers;
        }


        const unsigned int tail_start = head_bytes + (number_of_32_bit_integers * static_cast<unsigned int>(sizeof(std::uint32_t)));
        const unsigned int tail_bytes = bytes - tail_start;
        DEBUG_ASSERT(tail_bytes < sizeof(std::uint32_t));
        for (unsigned int i = 0U; i < tail_bytes; ++i) {
            WriteBits(std::bit_cast<std::uint8_t>(data[tail_start + i]), Constants::bits_per_byte);
        }

        DEBUG_ASSERT(GetAlignBits() == 0U);

        DEBUG_ASSERT(head_bytes + number_of_32_bit_integers * sizeof(std::uint32_t) + tail_bytes == bytes);
    }

    auto BitWriter::FlushBits() -> void {
        if (m_scratch_bits != 0U) {
            DEBUG_ASSERT(m_scratch_bits <= Constants::bits_per_uint32_t);
            DEBUG_ASSERT(m_32_bit_int_index < m_number_of_32_bit_ints);
            m_data[m_32_bit_int_index] =
                    HostToNetwork(static_cast<std::uint32_t>(m_scratch & std::numeric_limits<std::uint32_t>::max()));
            m_scratch >>= Constants::bits_per_uint32_t;
            m_scratch_bits = 0U;
            ++m_32_bit_int_index;
        }
    }

    auto BitWriter::GetAlignBits() const -> unsigned int {
        return (Constants::bits_per_byte - (m_bits_written % Constants::bits_per_byte)) % Constants::bits_per_byte;
    }

    auto BitWriter::GetBitsWritten() const -> unsigned int { return m_bits_written; }

    auto BitWriter::GetBitsAvailable() const -> unsigned int { return (m_number_of_32_bit_ints * Constants::bits_per_uint32_t) - m_bits_written; }

    auto BitWriter::GetData() const -> const std::byte * { return std::bit_cast<std::byte *>(m_data); }

    auto BitWriter::GetBytesWritten() const -> unsigned int {
        return (m_bits_written + (Constants::bits_per_byte - 1U)) / Constants::bits_per_byte;
    }
}
