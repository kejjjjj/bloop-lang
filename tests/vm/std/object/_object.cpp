#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("std.object.keys and values are in insertion order") {
    Expect("return std.object.keys({ b: 1, a: 2, c: 3 });", "[ b, a, c ]");
    Expect("return std.object.values({ b: 1, a: 2, c: 3 });", "[ 1, 2, 3 ]");
    Expect("let o = { x: 1 }; o.x = 5; return std.object.keys(o);", "[ x ]"); // overwriting doesn't change the order
    Expect("return std.object.keys({});", "[  ]");
    // order survives growing the table
    Expect("let o = {}; std.object.set(o, \"k1\", 1); std.object.set(o, \"k2\", 2); std.object.set(o, \"k3\", 3); std.object.set(o, \"k4\", 4);"
           "std.object.set(o, \"k5\", 5); std.object.set(o, \"k6\", 6); std.object.set(o, \"k7\", 7); return std.object.keys(o);",
           "[ k1, k2, k3, k4, k5, k6, k7 ]");
}
TEST_CASE("std.object.has and get") {
    Expect("let o = { a: 1 }; return [std.object.has(o, \"a\"), std.object.has(o, \"b\")];", "[ true, false ]");
    Expect("let o = { a: 1 }; return [std.object.get(o, \"a\"), std.object.get(o, \"b\")];", "[ 1, undefined ]");
    Expect("let o = { a: 1 }; let key = \"a\"; return std.object.get(o, key);", "1"); // dynamic keys
}
TEST_CASE("std.object.set") {
    Expect("let o = {}; std.object.set(o, \"name\", \"bloop\"); return o.name;", "bloop");
    Expect("let o = { a: 1 }; let r = std.object.set(o, \"a\", 2); return [r, o.a, std.length(o)];", "[ 2, 2, 1 ]");
    Expect("let o = {}; for (let i = 0; i < 200; i++) std.object.set(o, \"key\" + std.string.from(i), i); return [std.length(o), o.key0, o.key199];",
           "[ 200, 0, 199 ]");
}
TEST_CASE("std.object.remove") {
    Expect("let o = { a: 1, b: 2, c: 3 }; let r = std.object.remove(o, \"b\"); return [r, std.object.keys(o), std.length(o)];", "[ true, [ a, c ], 2 ]");
    Expect("let o = { a: 1 }; return std.object.remove(o, \"x\");", "false");
    Expect("let o = { a: 1 }; std.object.remove(o, \"a\"); std.object.set(o, \"a\", 2); return [o.a, std.length(o)];", "[ 2, 1 ]");
    // removing must keep every other key reachable (the table uses linear probing)
    Expect("let o = {}; for (let i = 0; i < 100; i++) std.object.set(o, std.string.from(i), i);"
           "for (let i = 0; i < 100; i += 2) std.object.remove(o, std.string.from(i));"
           "let ok = true; for (let i = 0; i < 100; i++) { let has = std.object.has(o, std.string.from(i)); if (has === (i % 2 === 0)) ok = false; }"
           "return [ok, std.length(o), std.object.get(o, \"99\")];", "[ true, 50, 99 ]");
    ExpectError("let o = { a: 1 }; std.object.remove(o, \"a\"); return o.a;", "unknown property \"a\"");
}
TEST_CASE("std.length of objects") {
    Expect("return [std.length({}), std.length({ a: 1, b: 2 })];", "[ 0, 2 ]");
}
TEST_CASE("std.object functions only accept objects and string keys") {
    ExpectError("return std.object.keys([1]);", "argument 1 must be \"object\", but got \"array\"");
    ExpectError("return std.object.get({}, 1);", "argument 2 must be \"string\" or \"handle\", but got \"int\"");
}
TEST_CASE("object literals keep the order their keys were written in") {
    Expect("return std.object.keys({ z: 1, y: 2, x: 3, w: 4, v: 5, u: 6 });", "[ z, y, x, w, v, u ]");
}
