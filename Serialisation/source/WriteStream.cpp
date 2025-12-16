#include <AlignmentUtility.hpp>
#include <WriteStream.hpp>
#include <Constant.hpp>

namespace Synapse::Serialise {
    WriteStream::WriteStream(std::uint32_t *buffer, const unsigned int bytes) : m_writer(buffer, bytes) {
        DEBUG_ASSERT(buffer != nullptr);
        DEBUG_ASSERT(Utility::IsAddressAligned(buffer, alignof(std::uint32_t)));
    }

    auto WriteStream::SerialiseBits(const std::uint32_t value, unsigned int bits) -> bool {
        DEBUG_ASSERT(bits > 0U);
        DEBUG_ASSERT(bits <= Constants::bits_per_uint32_t);
        m_writer.WriteBits(value, bits);
        return true;
    }

    auto WriteStream::SerialiseBytes(const std::byte *data, unsigned int bytes) -> bool {
        DEBUG_ASSERT(bytes > 0);
        DEBUG_ASSERT(data != nullptr);
        SerialiseAlign(); // ensures byte alignment before writing
        m_writer.WriteBytes(data, bytes);
        return true;
    }

    auto WriteStream::SerialiseAlign() -> void {
        m_writer.WriteZeroPaddingToAlignByteBoundary();
    }

    auto WriteStream::GetAlignBits() const -> unsigned int { return m_writer.GetAlignBits(); }

    auto WriteStream::Flush() -> void { m_writer.FlushBits(); }

    auto WriteStream::GetData() const -> const std::byte * { return m_writer.GetData(); }

    auto WriteStream::GetBytesProcessed() const -> unsigned int { return m_writer.GetBytesWritten(); }

    auto WriteStream::GetBitsProcessed() const -> unsigned int { return m_writer.GetBitsWritten(); }
}
