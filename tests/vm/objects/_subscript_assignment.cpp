#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// obj["key"] = value used to be an error for a missing key, while obj.key = value added it
TEST_CASE("objects | subscript assignment adds missing keys") {
    Expect("let o = {}; o[\"x\"] = 1; return o.x;", "1");
    Expect("let o = {}; const key = \"dynamic\"; o[key] = 5; return std.object.keys(o);", "[ dynamic ]");
    Expect("let o = { a: 1 }; o[\"a\"] = 2; o[\"b\"] = 3; return [o.a, o.b, std.object.keys(o)];", "[ 2, 3, [ a, b ] ]");
    Expect("let o = {}; o[\"n\"] = 1; o[\"n\"] += 5; o[\"n\"]++; return o.n;", "7");
    Expect("const o = {}; o[\"x\"] = o[\"y\"] = 2; return [o.x, o.y];", "[ 2, 2 ]"); // const only protects the variable
    Expect("let o = {}; for (let i = 0; i < 100; i++) { o[\"k\" + std.string.from(i)] = i; } return [std.length(o), o.k99];", "[ 100, 99 ]");
    // the assignment's value is the assigned value, even when the object had to grow (and the gc moved things)
    Expect("let o = {}; let last; for (let i = 0; i < 50; i++) { last = o[\"k\" + std.string.from(i)] = [i]; } return last;", "[ 49 ]");
}
TEST_CASE("objects | subscript assignment errors") {
    ExpectError("let o = {}; o[5] = 1;", "object index must be a string");
    ExpectError("let o = {}; return o[\"missing\"];", "unknown property \"missing\""); // reading is still strict
}
