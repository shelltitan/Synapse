
#include <catch2/catch.hpp>
#include <WriteStream.hpp>
#include <ReadStream.hpp>
#include <cstdint>
#include <array>
#include <bit>
#include <cstring>

using namespace Synapse::Serialise;

TEST_CASE("WriteStream and ReadStream roundtrip bools and bytes", "[stream][bool][bytes]") {
    constexpr unsigned int buffer_size_bytes = 32;
    std::array<std::uint32_t, buffer_size_bytes / sizeof(std::uint32_t)> buffer{};

    SECTION("Writing and reading booleans and byte arrays") {
        WriteStream writer(buffer.data(), buffer_size_bytes);

        bool value_true = true;
        bool value_false = false;
        std::array<std::byte, 5> input_bytes{ std::byte{0x01}, std::byte{0xAB}, std::byte{0xFF}, std::byte{0x11}, std::byte{0x22} };

        REQUIRE(writer.SerialiseBits(value_true, 1));
        REQUIRE(writer.SerialiseBits(value_false, 1));
        REQUIRE(writer.SerialiseBytes(input_bytes.data(), static_cast<unsigned int>(input_bytes.size())));
        writer.Flush();

        ReadStream reader(buffer.data(), buffer_size_bytes);
        bool out_true = false, out_false = true;
        REQUIRE(reader.DeserialiseBool(out_true));
        REQUIRE(reader.DeserialiseBool(out_false));
        REQUIRE(out_true == true);
        REQUIRE(out_false == false);

        std::array<std::byte, 5> output_bytes{};
        REQUIRE(reader.DeserialiseBytes(output_bytes.data(), static_cast<unsigned int>(output_bytes.size())));
        REQUIRE(output_bytes == input_bytes);
    }
}

TEST_CASE("WriteStream and ReadStream roundtrip sequence relative", "[stream][sequence][relative]") {
    constexpr unsigned int buffer_size_bytes = 32;
    std::array<std::uint32_t, buffer_size_bytes / sizeof(std::uint32_t)> buffer{};

    SECTION("Roundtrip relative sequence encoding") {
        WriteStream writer(buffer.data(), buffer_size_bytes);
        std::uint16_t seq1 = 1000;
        std::uint16_t seq2 = 1055;
        REQUIRE(writer.SerialiseUnsignedIntegerRelative(seq1, seq2));
        writer.Flush();

        ReadStream reader(buffer.data(), buffer_size_bytes);
        std::uint16_t decoded = 0;
        REQUIRE(reader.DeserialiseSequenceRelative(seq1, decoded));
        REQUIRE(decoded == seq2);
    }
}
