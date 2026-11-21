#include "tests/vm/std/helpers.hpp"
#include "tests/vm/defs.hpp"

#define PREFIX JOIN_PATH("operators", "logical")

TEST_CASE("logical and operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "logical_and"), Value::Type::t_bool, std::array{
        // bool && bool
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // int && int
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // uint && uint
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        // double && double
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
        bloop::BloopBool(false),
    });
}
TEST_CASE("logical or operations") {
    bloop::test::CheckArray(MAKE_RELATIVE_PATH(PREFIX, "logical_or"), Value::Type::t_bool, std::array{
        // bool || bool
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // int || int
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // uint || uint
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
        // double || double
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(true),
        bloop::BloopBool(false),
    });
}

// the right side only runs when the left one doesn't decide the result
TEST_CASE("logical operators short-circuit") {
	using namespace bloop::test::std_lib;
	// the guard pattern: the property isn't read when the value is undefined
	Expect("const by = undefined; return by !== undefined && by.kind === \"enemy\";", "false");
	Expect("const by = { kind: \"enemy\" }; return by !== undefined && by.kind === \"enemy\";", "true");
	Expect("const by = undefined; return by === undefined || by.kind === \"player\";", "true");
	// side effects of the skipped side don't happen
	Expect("let calls = 0; fn f() { calls++; return true; } "
	       "const a = false && f(); const b = true || f(); const c = true && f(); const d = false || f(); "
	       "return [a, b, c, d, calls];", "[ false, true, true, true, 2 ]");
	// the result is a boolean, from the truthiness of the operand that decided it
	Expect("return [1 && 2, 0 && 2, 1 && 0, 0 || 0, 0 || 3, 0.5 && true, undefined || undefined];",
	       "[ true, false, false, false, true, true, false ]");
	// (strings, arrays and objects aren't booleans, like in an if)
	ExpectError("return \"x\" && true;", "is not convertible to a boolean");
	ExpectError("return true && [];", "is not convertible to a boolean");
	// in conditions, loops and ternaries, and combined
	Expect("const arr = [1, 2, 3]; let i = 0; let sum = 0; while (i < std.length(arr) && arr[i] < 3) { sum += arr[i]; i++; } return sum;", "3");
	Expect("const o = undefined; if (o !== undefined && o.x > 1) { return \"no\"; } return \"guarded\";", "guarded");
	Expect("const t = true; const f = false; return [t && f || t, f || t && f, (f || t) && t, !(t && f)];", "[ true, false, true, true ]");
	Expect("const x = 5; return x > 3 && x < 10 ? \"in\" : \"out\";", "in");
	// as a statement (the value is thrown away)
	Expect("let n = 0; fn bump() { n++; return true; } true && bump(); false && bump(); false || bump(); return n;", "2");
}
