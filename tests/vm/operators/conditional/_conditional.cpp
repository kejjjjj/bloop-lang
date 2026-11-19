#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("conditional | picks a branch") {
    Expect("return true ? 1 : 2;", "1");
    Expect("return false ? 1 : 2;", "2");
    Expect("let x = 5; return x > 3 ? \"big\" : \"small\";", "big");
    Expect("return 0 ? \"a\" : undefined ? \"b\" : \"c\";", "c"); // same truthiness as if ()
}
TEST_CASE("conditional | precedence") {
    Expect("return 1 + 2 > 2 ? 10 + 1 : 20 + 1;", "11");                     // binds looser than arithmetic and comparisons
    Expect("return false || true ? \"yes\" : \"no\";", "yes");               // and than || / &&
    Expect("let y = 0; y = false ? 10 : 20; return y;", "20");               // but tighter than assignment
    Expect("let y = 1; y += true ? 10 : 20; return y;", "11");
    Expect("return (true ? 1 : 2) + 10;", "11");
}
TEST_CASE("conditional | chains are right associative") {
    Expect("fn f(n) { return n === 1 ? \"one\" : n === 2 ? \"two\" : \"many\"; } return [f(1), f(2), f(3)];", "[ one, two, many ]");
    Expect("return true ? false ? 1 : 2 : 3;", "2"); // a ? (b ? 1 : 2) : 3
    Expect("return false ? false ? 1 : 2 : 3;", "3");
    Expect("return false ? 1 : true ? 2 : 3;", "2");
}
TEST_CASE("conditional | inside other expressions") {
    Expect("return [true ? 1 : 2, false ? 3 : 4];", "[ 1, 4 ]");
    Expect("const o = { a: true ? 1 : 2, b: false ? 3 : 4 }; return [o.a, o.b];", "[ 1, 4 ]");
    Expect("fn g(a, b) { return a + b; } return g(true ? 1 : 2, false ? 3 : 4);", "5");
    Expect("let s = 0; for (let i = 0; i < 5; i++) { s += i % 2 === 0 ? i : 0; } return s;", "6");
    Expect("let r = []; for (const x : true ? [1, 2] : [3]) { std.array.push(r, x); } return r;", "[ 1, 2 ]");
    Expect("const f = (x) => x > 0 ? \"positive\" : \"other\"; return [f(1), f(-1)];", "[ positive, other ]");
    Expect("return std.array.transform([1, 2, 3], (x) => x % 2 === 0 ? x * 10 : x);", "[ 1, 20, 3 ]");
    Expect("return true ? { a: 1 }.a : 2;", "1");
}
TEST_CASE("conditional | only the picked branch runs") {
    Expect("let calls = 0; fn side() { calls++; return 1; } const a = true ? 5 : side(); const b = false ? side() : 6; return [a, b, calls];",
           "[ 5, 6, 0 ]");
    Expect("let o = { v: 1 }; true ? o.v++ : o.v--; false ? o.v++ : o.v--; return o.v;", "1"); // as a statement
    // conditions of loops and ifs drop the last instruction of the expression (its POP)
    Expect("let i = 0; while (i < 3 ? true : false) { i++; } return i;", "3");
    Expect("let n = 0; for (let i = 0; i < 10 ? i < 4 : false; i++) { n++; } return n;", "4");
    Expect("const x = 2; if (x > 1 ? x < 3 : false) { return \"in\"; } else if (!(x > 1) ? true : false) { return \"low\"; } return \"out\";", "in");
}
TEST_CASE("conditional | syntax errors") {
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return 1 : 2;"), Catch::Matchers::ContainsSubstring("\":\" without a \"?\""));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return true ? 1;"), Catch::Matchers::ContainsSubstring("expected a \":\" for this \"?\""));
    REQUIRE_THROWS(bloop::test::TEST_CompileBuffer("return true ? 1 : ;"));
    REQUIRE_THROWS(bloop::test::TEST_CompileBuffer("return true ? : 2;"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("let a = 1; (true ? a : a) = 2;"), Catch::Matchers::ContainsSubstring("lhs"));
}
