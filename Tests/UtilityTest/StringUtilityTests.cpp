#include <catch2/catch_test_macros.hpp>
#include <StringUtility.hpp>
#include <string>
#include <vector>

using namespace Synapse::Utility;

TEST_CASE("FilterString splits string on delimiter", "[FilterString]") {
    std::vector<std::string> tokens;

    SECTION("Basic split") {
        FilterString("a,b,c", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "a", "b", "c" });
    }

    SECTION("No delimiter present") {
        tokens.clear();
        FilterString("abc", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "abc" });
    }

    SECTION("Empty input string") {
        tokens.clear();
        FilterString("", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "" });
    }

    SECTION("Leading delimiter") {
        tokens.clear();
        FilterString(",a,b", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "", "a", "b" });
    }

    SECTION("Trailing delimiter") {
        tokens.clear();
        FilterString("a,b,", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "a", "b", "" });
    }

    SECTION("Multiple consecutive delimiters") {
        tokens.clear();
        FilterString("a,,b", ",", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "a", "", "b" });
    }

    SECTION("Multi-character delimiter") {
        tokens.clear();
        FilterString("a<>b<>c", "<>", tokens);
        REQUIRE(tokens == std::vector<std::string>{ "a", "b", "c" });
    }
}