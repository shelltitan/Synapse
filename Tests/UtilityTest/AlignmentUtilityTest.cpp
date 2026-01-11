#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <bit>
#include <AlignmentUtility.hpp>

using namespace Synapse::Utility;

TEST_CASE("IsSizeAligned returns true for aligned sizes", "[IsSizeAligned]") {
    REQUIRE(IsSizeAligned(16, 8)); // 16 is multiple of 8
    REQUIRE(IsSizeAligned(32, 16)); // 32 is multiple of 16
}

TEST_CASE("IsSizeAligned returns false for unaligned sizes", "[IsSizeAligned]") {
    REQUIRE_FALSE(IsSizeAligned(18, 8));
    REQUIRE_FALSE(IsSizeAligned(33, 16));
}

TEST_CASE("AlignSize aligns values up to next multiple of alignment", "[AlignSize]") {
    REQUIRE(AlignSize(13, 8) == 16);
    REQUIRE(AlignSize(16, 8) == 16);
    REQUIRE(AlignSize(17, 8) == 24);
    REQUIRE(AlignSize(0, 4) == 0);
}

TEST_CASE("IsAddressAligned works for aligned and unaligned addresses", "[IsAddressAligned]") {
    alignas(16) std::uint8_t aligned[32];
    std::uint8_t *ptr = aligned;

    REQUIRE(IsAddressAligned(ptr, 16));
    REQUIRE(IsAddressAligned(ptr + 16, 16));
    REQUIRE_FALSE(IsAddressAligned(ptr + 1, 16));
}

TEST_CASE("AlignAddress returns pointer aligned up to specified alignment", "[AlignAddress]") {
    alignas(16) std::uint8_t buffer[64];
    auto *base = reinterpret_cast<std::byte *>(buffer);

    auto *p_aligned = AlignAddress(base, 16);
    REQUIRE(IsAddressAligned(p_aligned, 16));
    REQUIRE(p_aligned == base); // now safe

    auto *p_offset = AlignAddress(base + 3, 16);
    REQUIRE(IsAddressAligned(p_offset, 16));
    REQUIRE(p_offset > base + 3);
    REQUIRE(p_offset < base + 3 + 16);
}