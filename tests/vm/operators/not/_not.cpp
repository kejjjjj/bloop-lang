#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("logical not | values") {
    Expect("return [!true, !false];", "[ false, true ]");
    Expect("return [!0, !1, !-5, !0.0, !0.5];", "[ true, false, false, true, false ]");
    Expect("return !undefined;", "true");
    Expect("return [!!5, !!0, !!!true];", "[ true, false, false ]");
}
TEST_CASE("logical not | in expressions") {
    Expect("let t = true; if (!t) { return 1; } return 2;", "2");
    Expect("return !(1 > 2) ? \"a\" : \"b\";", "a");
    Expect("return !false && !false;", "true");
    Expect("return !true || !false;", "true");
    Expect("return !(1 === 1) === false;", "true");
    Expect("let i = 0; while (!(i === 3)) { i++; } return i;", "3");
    Expect("return std.array.filter([1, 2, 3, 4], (x) => !(x % 2 === 0));", "[ 1, 3 ]");
    Expect("return !std.array.contains([1], 2);", "true");
}
TEST_CASE("logical not | values without a truth value are an error") {
    // the same rule as if (): arrays, objects, strings and functions aren't true or false
    ExpectError("return ![1];", "not convertible to a boolean");
    ExpectError("return !\"text\";", "not convertible to a boolean");
    ExpectError("return !{};", "not convertible to a boolean");
}
