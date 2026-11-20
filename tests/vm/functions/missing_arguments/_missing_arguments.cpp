#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// script functions are strict: they get exactly as many arguments as they have parameters
TEST_CASE("functions | too few arguments") {
    ExpectError("fn f(a, b) { return b; } f(1);", "passed 1 arguments, but expected 2");
    ExpectError("fn f(a, b, c) { return a; } f();", "passed 0 arguments, but expected 3");
    ExpectError("const f = (a, b) => b; f(1);", "passed 1 arguments, but expected 2"); // lambdas
    ExpectError("fn outer() { let x = 1; const f = (a, b) => x + a; return f(1); } outer();", "passed 1 arguments, but expected 2"); // closures
}
TEST_CASE("functions | too many arguments") {
    ExpectError("fn f(a) { return a; } f(1, 2);", "passed 2 arguments, but expected 1");
    ExpectError("const f = () => 1; f(1);", "passed 1 arguments, but expected 0");
}
TEST_CASE("functions | the right number of arguments") {
    Expect("fn f(a, b, c) { return [a, b, c]; } return f(1, 2, 3);", "[ 1, 2, 3 ]");
    Expect("fn f() { return 1; } return f();", "1");
    // std callbacks may declare fewer parameters than std passes
    Expect("return std.array.transform([1, 2], (x) => x * 10);", "[ 10, 20 ]");
}
