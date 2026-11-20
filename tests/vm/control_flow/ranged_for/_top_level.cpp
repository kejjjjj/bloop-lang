#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// inside functions the element is a local with a fresh binding per iteration. At the top level it's a global,
// and closures used to read it when they were called, so they all saw the last element
TEST_CASE("ranged for | closures at the top level keep their iteration's element") {
    Expect("const fs = []; for (const x : [1, 2, 3]) { std.array.push(fs, () => x); } return std.array.transform(fs, (f) => f());", "[ 1, 2, 3 ]");
    // nested loops and nested closures
    Expect("const fs = []; for (const x : [1, 2]) { for (const y : [10, 20]) { std.array.push(fs, () => () => x + y); } } "
           "return std.array.transform(fs, (f) => f()());", "[ 11, 21, 12, 22 ]");
    // functions declared in the body
    Expect("const fs = []; for (const x : [\"a\", \"b\"]) { fn get() { return x; } std.array.push(fs, get); } return std.array.transform(fs, (f) => f());",
           "[ a, b ]");
    // strings
    Expect("const fs = []; for (const c : \"ab\") { std.array.push(fs, () => c); } return std.array.transform(fs, (f) => f());", "[ a, b ]");
    // the same as inside a function
    Expect("fn run() { const fs = []; for (const x : [1, 2, 3]) { std.array.push(fs, () => x); } return std.array.transform(fs, (f) => f()); } return run();",
           "[ 1, 2, 3 ]");
}
TEST_CASE("ranged for | top level closures and the gc") {
    Expect("const fs = []; for (const x : [[1], [2], [3]]) { std.array.push(fs, () => x); std.gc_minor(); } std.gc_major(); "
           "return std.array.transform(fs, (f) => f()[0]);", "[ 1, 2, 3 ]");
}
