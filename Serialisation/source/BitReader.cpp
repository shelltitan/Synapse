#include <BitReader.hpp>
#include <AlignmentUtility.hpp>
#include <Constant.hpp>
#include <SerialiseBit.hpp>
#include <algorithm>
#include <cstring>
#include <libassert/assert.hpp>

namespace Synapse::Serialise {
    BitReader::BitReader(const std::uint32_t *data, const unsigned int bytes) :
        m_bitpacked_data(data), m_number_of_bytes(bytes)
#ifdef _DEBUG
        ,
        m_number_of_32_bit_ints(static_cast<std::uint32_t>(Utility::AlignSize(bytes, alignof(std::uint32_t))))
#endif
    {
        DEBUG_ASSERT(data != nullptr);
        DEBUG_ASSERT(Utility::IsAddressAligned(data, alignof(std::uint32_t)));
        DEBUG_ASSERT((bytes % sizeof(std::uint32_t)) == 0U);
        DEBUG_ASSERT(bytes > 0U);
    }

    auto BitReader::WouldReadPastEnd(const unsigned int bits) const -> bool {
        return (m_bits_read + bits) > (m_number_of_bytes * Constants::bits_per_byte);
    }

    auto BitReader::ReadBits(const unsigned int bits) -> std::uint32_t {
        DEBUG_ASSERT(bits > 0U);
        DEBUG_ASSERT(bits <= Constants::bits_per_uint32_t);
        DEBUG_ASSERT((m_bits_read + bits) <= (m_number_of_bytes * Constants::bits_per_byte));

        m_bits_read += bits;

        DEBUG_ASSERT(m_scratch_bits <= Constants::bits_per_uint64_t);

        if (m_scratch_bits < bits) {
            DEBUG_ASSERT(m_32_bit_int_index < m_number_of_32_bit_ints);
            m_scratch |= static_cast<std::uint64_t>(NetworkToHost(m_bitpacked_data[m_32_bit_int_index]))
                         << m_scratch_bits;
            m_scratch_bits += Constants::bits_per_uint32_t;
            ++m_32_bit_int_index;
        }

        DEBUG_ASSERT(m_scratch_bits >= bits);

        const auto output = static_cast<std::uint32_t>(m_scratch & ((static_cast<uint64_t>(1) << bits) - 1U));

        m_scratch >>= bits;
        m_scratch_bits -= bits;

        return output;
    }

    auto BitReader::SkipToByteBoundaryAndVerifyZeroPadding() -> bool {
        if (const unsigned int remainder_bits = m_bits_read % Constants::bits_per_byte; remainder_bits != 0U) {
            const std::uint32_t value = ReadBits(Constants::bits_per_byte - remainder_bits);
            DEBUG_ASSERT((m_bits_read % Constants::bits_per_byte) == 0U);
            if (value != 0U) {
                return false;
            }
        }
        return true;
    }

    auto BitReader::ReadBytes(std::byte* data, const unsigned int bytes) -> void {
        DEBUG_ASSERT(bytes > 0U);
        DEBUG_ASSERT(GetAlignBits() == 0U);
        DEBUG_ASSERT((m_bits_read + (bytes * Constants::bits_per_byte)) <= (m_number_of_bytes * Constants::bits_per_byte));
        DEBUG_ASSERT((m_bits_read % Constants::bits_per_byte) == 0U);

        // We calculate the number of bytes until we align on a 32-bit unsigned integer boundary
        unsigned int head_bytes =
                (static_cast<unsigned int>(sizeof(std::uint32_t)) - (m_bits_read % Constants::bits_per_uint32_t) / Constants::bits_per_byte) %
                static_cast<unsigned int>(sizeof(std::uint32_t));
        if (head_bytes > bytes) {
            head_bytes = bytes;
        }
        for (unsigned int i = 0U; i < head_bytes; ++i) {
            data[i] = std::bit_cast<std::byte>(static_cast<std::uint8_t>(ReadBits(Constants::bits_per_byte)));
        }
        if (head_bytes == bytes) {
            return;
        }

        const unsigned int number_of_32_bit_integers = (bytes - head_bytes) / static_cast<unsigned int>(sizeof(std::uint32_t));
        if (number_of_32_bit_integers > 0U) {
            DEBUG_ASSERT((m_bits_read % Constants::bits_per_uint32_t) == 0U);
            (void) std::copy_n(std::bit_cast<std::byte *>(&m_bitpacked_data[number_of_32_bit_integers]),
                    number_of_32_bit_integers * sizeof(std::uint32_t),
                    data + head_bytes);
            m_bits_read += number_of_32_bit_integers * Constants::bits_per_uint32_t;
            m_32_bit_int_index += number_of_32_bit_integers;
        }

        const unsigned int tail_start = head_bytes + number_of_32_bit_integers * static_cast<unsigned int>(sizeof(std::uint32_t));
        const unsigned tail_bytes = bytes - tail_start;
        DEBUG_ASSERT(tail_bytes < sizeof(std::uint32_t));
        for (unsigned int i = 0U; i < tail_bytes; ++i) {
            data[tail_start + i] = std::bit_cast<std::byte>(static_cast<std::uint8_t>(ReadBits(Constants::bits_per_byte)));
        }

        DEBUG_ASSERT((head_bytes + (number_of_32_bit_integers * sizeof(std::uint32_t)) + tail_bytes) == bytes);
    }

    auto BitReader::GetAlignBits() const -> unsigned int {
        return (Constants::bits_per_byte - (m_bits_read % Constants::bits_per_byte)) % Constants::bits_per_byte;
    }

    auto BitReader::GetBitsRead() const -> unsigned int { return m_bits_read; }

    auto BitReader::GetBitsRemaining() const -> unsigned int {
        return m_number_of_bytes * Constants::bits_per_byte - m_bits_read;
    }
}
