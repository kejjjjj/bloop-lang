#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("typeof | primitive values") {
    Expect("return [typeof undefined, typeof true, typeof 1, typeof 1u, typeof 1.5];", "[ undefined, boolean, int, uint, double ]");
    Expect("return typeof \"text\";", "string");
    Expect("return typeof `a ${1} b`;", "string");
}
TEST_CASE("typeof | objects and functions") {
    Expect("return [typeof [], typeof {}];", "[ array, object ]");
    Expect("fn f() { return 1; } return typeof f;", "function");
    Expect("return typeof (() => 1);", "function");
    Expect("let n = 1; const g = () => n; return typeof g;", "function"); // a closure
    Expect("return typeof std.console.log;", "function");
    Expect("return typeof std.coroutine.create(() => 1);", "coroutine");
    Expect("return typeof std.task.spawn(() => { std.task.wait(1); });", "task"); // a handle: its type name
}
TEST_CASE("typeof | precedence") {
    // a prefix operator: it binds tighter than binary operators and takes the whole postfix chain
    Expect("const o = { a: [1.5] }; return [typeof o.a, typeof o.a[0]];", "[ array, double ]");
    Expect("return typeof 1 == \"int\";", "true");
    Expect("return typeof 1 + \"!\";", "int!");
    Expect("return [typeof -1, typeof !1, typeof typeof 1, typeof (1 + 0.5)];", "[ int, boolean, string, double ]");
    Expect("const k = (v) => typeof v == \"string\" ? \"text\" : typeof v; return [k(\"a\"), k([])];", "[ text, array ]");
}
TEST_CASE("typeof | as a statement and in conditions") {
    Expect("typeof 1; return 2;", "2");
    Expect("fn f() { return 1; } if (typeof f == \"function\") { return 1; } return 0;", "1");
}
TEST_CASE("typeof | is a keyword") {
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("const typeof = 1;"), Catch::Matchers::ContainsSubstring("expected an identifier"));
    REQUIRE_THROWS(bloop::test::TEST_CompileBuffer("const t = typeof;"));
    Expect("const o = { \"typeof\": 1 }; return o[\"typeof\"];", "1");
}
