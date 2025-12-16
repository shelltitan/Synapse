#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <ReadStream.hpp>
#include <WriteStream.hpp>
#include <array>

TEST_CASE("DeserialiseUnsignedIntegerRelative<T> works for uint16_t and uint32_t", "[serialization]") {
    using namespace Synapse::Serialise;

    SECTION("Round-trip test for uint16_t deltas") {
        std::uint16_t previous = 1000;

        std::array<std::uint16_t, 6> test_deltas = { 1U, 3U, 10U, 100U, 1000U, 30000U };

        for (auto delta: test_deltas) {
            std::uint16_t value = previous + delta;

            BitWriter writer;
            SerialiseUnsignedIntegerRelative(previous, value, writer);

            BitReader reader(writer.GetData(), writer.GetBitPosition() / 8);
            std::uint16_t decoded = 0;
            REQUIRE(DeserialiseUnsignedIntegerRelative(previous, decoded));
            REQUIRE(decoded == value);
        }
    }

    SECTION("Round-trip test for uint32_t deltas") {
        std::uint32_t previous = 1'000'000;

        std::array<std::uint32_t, 6> test_deltas = { 1U, 4U, 20U, 300U, 5000U, 100000U };

        for (auto delta: test_deltas) {
            std::uint32_t value = previous + delta;

            BitWriter writer;
            SerialiseUnsignedIntegerRelative(previous, value, writer);

            BitReader reader(writer.GetData(), writer.GetBitPosition() / 8);
            std::uint32_t decoded = 0;
            REQUIRE(DeserialiseUnsignedIntegerRelative(previous, decoded));
            REQUIRE(decoded == value);
        }
    }
}