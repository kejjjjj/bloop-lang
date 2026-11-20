#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("bitwise not") {
    Expect("return [~5, ~0, ~-1, ~~42];", "[ -6, -1, 0, 42 ]");
    Expect("return ~9223372036854775807;", "-9223372036854775808"); // 64-bit everywhere
    Expect("let x = 12; return (~x & 15) === 3;", "true");
    Expect("return -~5;", "6");
    ExpectError("return ~1.5;", "value of type \"double\" isn't an integer");
    ExpectError("return ~\"a\";", "isn't an integer");
    ExpectError("return ~true;", "isn't an integer");
}
