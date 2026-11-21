#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("number literals | exponents") {
    Expect("return [1e6, 1E3, 2.5e-3, 1e+2, .5e1, 1_000e3];", "[ 1000000.000000, 1000.000000, 0.002500, 100.000000, 5.000000, 1000000.000000 ]");
    Expect("return [1e0 === 1.0, 25e-1 === 2.5, 1e300 > 1e299];", "[ true, true, true ]");
    // an exponent makes a double, even without a decimal point
    Expect("return [typeof 1e3, typeof 1, typeof 1.0];", "[ double, int, double ]");
    Expect("return 1e2 * 2;", "200.000000");
    Expect("return 2.5e-3 * 1e3;", "2.500000");
}
TEST_CASE("number literals | hexadecimal digits are not exponents") {
    Expect("return [0x1e5, 0xE, 0xe];", "[ 485, 14, 14 ]");
}
TEST_CASE("number literals | malformed exponents") {
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return 1e;"), Catch::Matchers::ContainsSubstring("exponent's digits"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return 1e+;"), Catch::Matchers::ContainsSubstring("exponent's digits"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return 2.5E-;"), Catch::Matchers::ContainsSubstring("exponent's digits"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return 1e999;"), Catch::Matchers::ContainsSubstring("out of range"));
    REQUIRE_THROWS(bloop::test::TEST_CompileBuffer("return 1e3u;"));
}
