#include <SerialiseBit.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <array>
#include <type_traits>
using namespace Synapse::Serialise;

TEST_CASE("BitsRequired - Valid range calculations", "[bit_utils]") {
    SECTION("Zero range") {
        REQUIRE(BitsRequired(0, 0) == 0);
        REQUIRE(BitsRequired(5, 5) == 0);
        REQUIRE(BitsRequired(100, 100) == 0);
    }

    SECTION("Increasing ranges") {
        REQUIRE(BitsRequired(0, 1) == 1); // range = 1 → 1 bit
        REQUIRE(BitsRequired(0, 2) == 2); // range = 2 → 2 bits
        REQUIRE(BitsRequired(0, 3) == 2); // range = 3 → 2 bits
        REQUIRE(BitsRequired(0, 4) == 3); // range = 4 → 3 bits
        REQUIRE(BitsRequired(5, 6) == 1); // range = 1 → 1 bit
        REQUIRE(BitsRequired(5, 7) == 2); // range = 2 → 2 bits
        REQUIRE(BitsRequired(5, 12) == 4); // range = 7 → 3 bits
    }

    SECTION("Power-of-two boundaries") {
        REQUIRE(BitsRequired(0, 255) == 8);
        REQUIRE(BitsRequired(0, 256) == 9);
        REQUIRE(BitsRequired(0, 1023) == 10);
        REQUIRE(BitsRequired(0, 1024) == 11);
    }

    SECTION("Full 32-bit range") {
        REQUIRE(BitsRequired(0, std::numeric_limits<std::uint32_t>::max()) == 32);
        REQUIRE(BitsRequired(123, std::numeric_limits<std::uint32_t>::max()) == 32);
    }

    SECTION("Invalid ranges") {
        REQUIRE(BitsRequired(100, 50) == 0); // min > max
        REQUIRE(BitsRequired(9999, 9998) == 0);
    }

    SECTION("Constexpr evaluation") {
        constexpr unsigned int bits = BitsRequired(0, 15);
        STATIC_REQUIRE(bits == 4);
    }
}

TEST_CASE("SignedToUnsigned ZigZag encoding", "[zigzag]") {
    SECTION("Basic values") {
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(0) == 0u);
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(-1) == 1u);
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(1) == 2u);
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(-2) == 3u);
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(2) == 4u);
    }

    SECTION("Large and extreme values") {
        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(std::numeric_limits<std::int32_t>::max()) ==
                static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) << 1);

        REQUIRE(ZigZagEncodeSignedToUnsigned<std::int32_t>(std::numeric_limits<std::int32_t>::min()) ==
                        ((static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::min()) << 1) ^
                0xFFFFFFFF));
    }

    SECTION("Type safety and constexpr") {
        constexpr uint32_t result = ZigZagEncodeSignedToUnsigned<std::int32_t>(-10);
        STATIC_REQUIRE(result == 19);
    }
}

TEST_CASE("UnsignedToSigned ZigZag decoding", "[zigzag]") {
    SECTION("Basic values") {
        REQUIRE(ZigZagDecodeUnsignedToSigned<uint32_t>(0u) == 0);
        REQUIRE(ZigZagDecodeUnsignedToSigned<uint32_t>(1u) == -1);
        REQUIRE(ZigZagDecodeUnsignedToSigned<uint32_t>(2u) == 1);
        REQUIRE(ZigZagDecodeUnsignedToSigned<uint32_t>(3u) == -2);
        REQUIRE(ZigZagDecodeUnsignedToSigned<uint32_t>(4u) == 2);
    }

    SECTION("Large and extreme values") {
        constexpr std::int32_t max = std::numeric_limits<std::int32_t>::max();
        constexpr std::int32_t min = std::numeric_limits<std::int32_t>::min();

        const uint32_t encoded_max = static_cast<uint32_t>(max) << 1;
        REQUIRE(ZigZagDecodeUnsignedToSigned(encoded_max) == max);

        const uint32_t encoded_min = static_cast<uint32_t>(min) << 1 ^ 0xFFFFFFFF;
        REQUIRE(ZigZagDecodeUnsignedToSigned(encoded_min) == min);
    }

    SECTION("Type safety and constexpr") {
        constexpr std::int32_t result = ZigZagDecodeUnsignedToSigned<uint32_t>(19u);
        STATIC_REQUIRE(result == -10);
    }
}

TEST_CASE("ZigZag encoding and decoding roundtrip", "[zigzag]") {
    SECTION("int32_t <-> uint32_t") {
        for (std::int32_t original: { 0, 1, -1, 2, -2, 123, -123, std::numeric_limits<std::int32_t>::max(),
                     std::numeric_limits<std::int32_t>::min() + 1 }) {
            std::uint32_t encoded = ZigZagEncodeSignedToUnsigned(original);
            std::int32_t decoded = ZigZagDecodeUnsignedToSigned(encoded);
            REQUIRE(decoded == original);
        }
    }

    SECTION("int16_t <-> uint16_t") {
        for (std::int16_t original: std::array<std::int16_t, 9>{ 0, 1, -1, 2, -2, 123, -123,
                     std::numeric_limits<std::int16_t>::max(),
                     std::numeric_limits<std::int16_t>::min() + 1 }) {
            std::uint16_t encoded = ZigZagEncodeSignedToUnsigned(original);
            std::int16_t decoded = ZigZagDecodeUnsignedToSigned(encoded);
            REQUIRE(decoded == original);
        }
    }
}

TEST_CASE("Relative sequence encoding bits are computed correctly", "[bitcalc]") {

    SECTION("One-bit delta") { REQUIRE(GetRelativeSequenceEncodingBits(100, 101) == 1); }

    SECTION("Two-bit prefix range (delta in [2, 5])") {
        REQUIRE(GetRelativeSequenceEncodingBits(100, 102) == 2 + BitsRequired(2, 5));
        REQUIRE(GetRelativeSequenceEncodingBits(100, 105) == 2 + BitsRequired(2, 5));
    }

    SECTION("Three-bit prefix range (delta in [6, 21])") {
        REQUIRE(GetRelativeSequenceEncodingBits(100, 106) == 3 + BitsRequired(6, 21));
        REQUIRE(GetRelativeSequenceEncodingBits(100, 121) == 3 + BitsRequired(6, 21));
    }

    SECTION("Four-bit prefix range (delta in [22, 277])") {
        REQUIRE(GetRelativeSequenceEncodingBits(100, 122) == 4 + BitsRequired(22, 277));
        REQUIRE(GetRelativeSequenceEncodingBits(100, 377) == 4 + BitsRequired(22, 277));
    }

    SECTION("Five-bit prefix range (delta in [278, 4373])") {
        REQUIRE(GetRelativeSequenceEncodingBits(100, 378) == 5 + BitsRequired(278, 4373));
        REQUIRE(GetRelativeSequenceEncodingBits(100, 4473) == 5 + BitsRequired(278, 4373));
    }

    SECTION("Six-bit prefix range (delta in [4374, 69909])") {
        REQUIRE(GetRelativeSequenceEncodingBits(100, 4474) == 6 + BitsRequired(4374, 69909));
        REQUIRE(GetRelativeSequenceEncodingBits(100, 70000 % 65536) == 6 + BitsRequired(4374, 69909));
    }

    SECTION("Fallback to 32-bit encoding") {
        REQUIRE(GetRelativeSequenceEncodingBits(0, 65535) == Constants::bits_per_uint32_t);
        REQUIRE(GetRelativeSequenceEncodingBits(1, 65535) == Constants::bits_per_uint32_t);
    }

    SECTION("Wraparound cases") {
        REQUIRE(GetRelativeSequenceEncodingBits(65000, 1000) == 5 + BitsRequired(278, 4373)); // delta = 1536
        REQUIRE(GetRelativeSequenceEncodingBits(65535, 0) == 1); // delta = 1 with wraparound
        REQUIRE(GetRelativeSequenceEncodingBits(65530, 5) == 3 + BitsRequired(6, 21)); // delta = 11
    }

    SECTION("No difference (equal sequences)") {
        REQUIRE(GetRelativeSequenceEncodingBits(1000, 1000) == Constants::bits_per_uint32_t);
        REQUIRE(GetRelativeSequenceEncodingBits(0, 0) == Constants::bits_per_uint32_t);
        REQUIRE(GetRelativeSequenceEncodingBits(65535, 65535) == Constants::bits_per_uint32_t);
    }

    SECTION("Constexpr correctness") {
        constexpr unsigned int bits = GetRelativeSequenceEncodingBits(1000, 1001);
        STATIC_REQUIRE(bits == 1);
    }
}
