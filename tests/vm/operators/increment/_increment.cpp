#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// ++ and -- used to only work on variables: on obj.key or arr[i] they checked whether the object's
// variable was const, and stored the result into that variable (o.a++ replaced o with a number)
TEST_CASE("increment | variables") {
    Expect("let i = 0; i++; ++i; return i;", "2");
    Expect("let i = 5; const a = i++; const b = ++i; const c = i--; const d = --i; return [a, b, c, d, i];", "[ 5, 7, 7, 5, 5 ]");
    Expect("fn f() { let i = 1; i++; return i; } return f();", "2");
    Expect("fn f() { let i = 1; const g = () => { i++; return i; }; g(); return g(); } return f();", "3"); // upvalues
}
TEST_CASE("increment | properties") {
    Expect("let o = { a: 1 }; o.a++; return o.a;", "2");
    Expect("let o = { a: 1 }; const before = o.a++; return [before, o.a];", "[ 1, 2 ]");
    Expect("let o = { a: 1 }; const after = ++o.a; return [after, o.a];", "[ 2, 2 ]");
    Expect("let o = { a: 1 }; o.a--; --o.a; return o.a;", "-1");
    Expect("let o = { a: { n: 1 } }; const r = o.a.n++; const q = --o.a.n; return [r, q, o.a.n];", "[ 1, 1, 1 ]");
    // the object stays an object
    Expect("let o = { a: 1 }; o.a++; return std.object.keys(o);", "[ a ]");
}
TEST_CASE("increment | elements") {
    Expect("let a = [1, 5]; a[1]--; const b = a[0]++; const c = ++a[0]; return [a, b, c];", "[ [ 3, 4 ], 1, 3 ]");
    Expect("let a = [[1]]; a[0][0]++; return a;", "[ [ 2 ] ]");
    Expect("let a = [0, 0, 0]; for (let i = 0; i < 3; i++) { a[i] += i; a[i]++; } return a;", "[ 1, 2, 3 ]");
}
TEST_CASE("increment | elements and properties of const variables") {
    // const only means that the variable can't change, like in js
    Expect("const o = { count: 0 }; o.count++; ++o.count; return o.count;", "2");
    Expect("const a = [1]; a[0]--; return a[0];", "0");
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("const x = 1; x++;"), Catch::Matchers::ContainsSubstring("can't increment a const value"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("const x = 1; --x;"), Catch::Matchers::ContainsSubstring("can't decrement a const value"));
}
TEST_CASE("increment | invalid operands") {
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("fn f() { return 1; } f()++;"), Catch::Matchers::ContainsSubstring("must be a variable, an element or a property"));
    REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("5++;"), Catch::Matchers::ContainsSubstring("must be a variable, an element or a property"));
    ExpectError("let s = \"a\"; s++;", "not arithmetic");
}
