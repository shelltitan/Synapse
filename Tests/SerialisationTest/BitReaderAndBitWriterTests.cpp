#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>
#include "BitReader.hpp"
#include "BitWriter.hpp"
#include "Serialise.hpp"

using namespace Synapse::Serialise;

TEST_CASE("DeserialiseUnsignedIntegerRelative works correctly", "[serialisation][relative]") {
    std::vector<std::uint8_t> buffer(32);
    BitWriter writer(reinterpret_cast<std::uint32_t *>(buffer.data()), buffer.size());

    SECTION("Relative deltas roundtrip") {
        std::uint32_t previous = 1000;
        std::vector<std::uint32_t> deltas = { 1, 3, 10, 50, 300, 7000 };

        for (auto delta: deltas) {
            std::uint32_t current = previous + delta;

            REQUIRE(SerialiseUnsignedIntegerRelative(previous, current, writer));
            previous = current;
        }

        writer.FlushBits();

        BitReader reader(reinterpret_cast<const std::uint32_t *>(buffer.data()), writer.GetBytesWritten());
        previous = 1000;

        for (auto delta: deltas) {
            std::uint32_t current;
            REQUIRE(reader.DeserialiseUnsignedIntegerRelative(previous, current));
            REQUIRE(current == previous + delta);
            previous = current;
        }
    }

    SECTION("Wraparound case") {
        std::uint16_t prev = 65530;
        std::uint16_t curr = 5; // implies wraparound

        REQUIRE(SerialiseUnsignedIntegerRelative(prev, curr, writer));
        writer.FlushBits();

        BitReader reader(reinterpret_cast<const std::uint32_t *>(buffer.data()), writer.GetBytesWritten());
        std::uint16_t out;
        REQUIRE(reader.DeserialiseUnsignedIntegerRelative(prev, out));
        REQUIRE(out == curr);
    }
}