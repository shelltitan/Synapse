
#include <catch2/catch.hpp>
#include <BitReader.hpp>
#include <BitWriter.hpp>
#include <cstdint>
#include <array>
#include <bit>

using namespace Synapse::Serialise;

TEST_CASE("BitWriter and BitReader roundtrip bits", "[bit][roundtrip]") {
    constexpr unsigned int buffer_size_bytes = 16;
    std::array<std::uint32_t, buffer_size_bytes / sizeof(std::uint32_t)> buffer{};

    BitWriter writer(buffer.data(), buffer_size_bytes);
    writer.WriteBits(0b1011, 4);
    writer.WriteBits(0xFF, 8);
    writer.WriteZeroPaddingToAlignByteBoundary();

    writer.WriteBits(0xA, 4);
    writer.WriteBits(0x1, 1);
    writer.FlushBits();

    BitReader reader(buffer.data(), buffer_size_bytes);
    REQUIRE(reader.ReadBits(4) == 0b1011);
    REQUIRE(reader.ReadBits(8) == 0xFF);
    REQUIRE(reader.SkipToByteBoundaryAndVerifyZeroPadding());
    REQUIRE(reader.ReadBits(4) == 0xA);
    REQUIRE(reader.ReadBits(1) == 0x1);
}

TEST_CASE("BitWriter and BitReader roundtrip bytes", "[byte][roundtrip]") {
    constexpr unsigned int buffer_size_bytes = 32;
    std::array<std::uint32_t, buffer_size_bytes / sizeof(std::uint32_t)> buffer{};

    std::array<std::byte, 11> input_bytes{};
    for (std::size_t i = 0; i < input_bytes.size(); ++i)
        input_bytes[i] = std::byte(0xA0 + i);

    BitWriter writer(buffer.data(), buffer_size_bytes);
    writer.WriteZeroPaddingToAlignByteBoundary();
    writer.WriteBytes(input_bytes.data(), static_cast<unsigned int>(input_bytes.size()));
    writer.FlushBits();

    BitReader reader(buffer.data(), buffer_size_bytes);
    REQUIRE(reader.SkipToByteBoundaryAndVerifyZeroPadding());

    std::array<std::byte, 11> output_bytes{};
    reader.ReadBytes(output_bytes.data(), static_cast<unsigned int>(output_bytes.size()));

    REQUIRE(output_bytes == input_bytes);
}
