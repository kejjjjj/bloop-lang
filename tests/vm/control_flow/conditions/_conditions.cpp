#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// conditions used to be emitted as statements and then have their last instruction (the POP) removed
TEST_CASE("conditions | for loops without a condition") {
    Expect("let n = 0; for (;;) { n++; if (n === 3) { break; } } return n;", "3");
    Expect("let n = 0; for (let i = 0; ; i++) { n += i; if (i === 4) { break; } } return n;", "10");
    Expect("fn f() { for (;;) { return 7; } } return f();", "7");
    Expect("let n = 0; for (;;) { n++; if (n < 5) { continue; } break; } return n;", "5");
}
TEST_CASE("conditions | declarations as conditions") {
    Expect("let r = 0; if (const x = 5) { r = x; } return r;", "5");
    Expect("let r = 0; if (const x = 0) { r = 1; } else { r = 2; } return r;", "2");
    Expect("let n = 0; while (let go = n < 3) { n++; } return n;", "3");
}
TEST_CASE("conditions | any expression") {
    Expect("let o = { a: 1 }; if (o.a++) { return o.a; } return -1;", "2");
    Expect("let i = 0; let n = 0; while (i++ < 3) { n++; } return [i, n];", "[ 4, 3 ]");
    Expect("let x = 0; if (x = 5) { return x; } return -1;", "5");
    Expect("let a = [0]; let n = 0; while (++a[0] < 4) { n++; } return [a[0], n];", "[ 4, 3 ]");
    Expect("let i = 0; while (!(i === 3)) { i++; } return i;", "3");
    Expect("let r = []; for (let i = 0; i < 6; i++) { if (i % 2 === 0 ? i < 4 : false) { std.array.push(r, i); } } return r;", "[ 0, 2 ]");
}
