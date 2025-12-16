#include <libassert/assert.hpp>
#include <ReadStream.hpp>
#include <AlignmentUtility.hpp>

namespace Synapse::Serialise {
    ReadStream::ReadStream(const std::uint32_t* buffer, const unsigned int bytes) : m_reader(buffer, bytes) {
        DEBUG_ASSERT(buffer != nullptr);
        DEBUG_ASSERT(Utility::IsAddressAligned(buffer, alignof(std::uint32_t)));
    }

    auto ReadStream::DeserialiseBytes(std::byte *data, const unsigned int bytes) -> bool {
        if (!DeserialiseAlign()) {
            return false;
        }
        if (m_reader.WouldReadPastEnd(bytes * Constants::bits_per_byte)) {
            return false;
        }
        m_reader.ReadBytes(data, bytes);
        return true;
    }

    auto ReadStream::DeserialiseAlign() -> bool {

        if (const unsigned int align_bits = m_reader.GetAlignBits(); m_reader.WouldReadPastEnd(align_bits)) {
            return false;
        }

        if (!m_reader.SkipToByteBoundaryAndVerifyZeroPadding()) {
            return false;
        }

        return true;
    }

    auto ReadStream::DeserialiseBool(bool &value) -> bool {
        std::uint32_t uint32_bool_value = 0U;
        if (!DeserialiseBits(uint32_bool_value, 1U)) {
            return false;
        }
        value = (uint32_bool_value != 0U);
        return true;
    }

    auto ReadStream::DeserialiseSequenceRelative(const std::uint16_t sequence1, std::uint16_t &sequence2) -> bool {
        const std::uint32_t a = sequence1;
        std::uint32_t b = 0U;

        if (!DeserialiseUnsignedIntegerRelative(a, b)) {
            return false;
        }

        sequence2 = static_cast<std::uint16_t>(b); // automatic wraparound
        return true;
    }

    auto ReadStream::GetAlignBits() const -> unsigned int {
        return m_reader.GetAlignBits();
    }

    auto ReadStream::GetBitsProcessed() const -> unsigned int {
        return m_reader.GetBitsRead();
    }

    auto ReadStream::GetBytesProcessed() const -> unsigned int {
        return (m_reader.GetBitsRead() + 7U) / Constants::bits_per_byte;
    }
}