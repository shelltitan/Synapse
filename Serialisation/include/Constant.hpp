#pragma once
#include <climits>
#include <cstdint>

namespace Synapse::Serialise::Constants {
    /// Number of bits in a byte (usually 8).
    /// \todo replace charbits with this everywhere
    constexpr unsigned int bits_per_byte = static_cast<unsigned int>(CHAR_BIT);

    /// Number of bits in a 16-bit unsigned integer.
    constexpr unsigned int bits_per_uint16_t = bits_per_byte * sizeof(std::uint16_t);

    /// Number of bits in a 32-bit unsigned integer.
    constexpr unsigned int bits_per_uint32_t = bits_per_byte * sizeof(std::uint32_t);

    /// Number of bits in a 64-bit unsigned integer.
    constexpr unsigned int bits_per_uint64_t = bits_per_byte * sizeof(std::uint64_t);
}
