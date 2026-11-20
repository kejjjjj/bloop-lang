#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// script functions calling each other don't recurse in C++: CALL pushes a frame and the same loop carries on,
// RETURN pops it. Only natives calling back into the script (std.array.sort, the host...) nest the loop

TEST_CASE("calls | recursion as deep as the frame limit allows, in every build") {
	// (before, a debug build with sanitizers ran out of native stack after a few hundred calls)
	Expect("fn depth(n) { if (n === 0) { return 0; } return depth(n - 1) + 1; } return depth(1000);", "1000");
	Expect("fn even(n) { return n === 0 ? true : odd(n - 1); } fn odd(n) { return n === 0 ? false : even(n - 1); } return even(1000);", "true");
	// lambdas and closures too
	Expect("const down = (n) => n === 0 ? \"done\" : down(n - 1); return down(1000);", "done");
	Expect("fn make() { let calls = 0; fn f(n) { calls++; return n === 0 ? calls : f(n - 1); } return f; } return make()(999);", "1000");
}

TEST_CASE("calls | too deep is a runtime error about the frames") {
	ExpectError("fn f(n) { return f(n + 1); } return f(0);", "exceeded 1024 call frames");
	ExpectError("const f = (n) => f(n + 1); return f(0);", "exceeded 1024 call frames");
}

TEST_CASE("calls | the script keeps running after a deep call returns") {
	Expect("fn depth(n) { return n === 0 ? 0 : depth(n - 1) + 1; } let total = 0; "
	       "for (let i = 0; i < 20; i++) { total += depth(900); } return total;", "18000");
}

TEST_CASE("calls | values and locals of every frame survive the calls in between") {
	Expect("fn add3(a, b, c) { const x = a * 100; const y = b * 10; return x + y + c; } "
	       "fn outer(a) { const before = a; const r = add3(a, add3(1, 2, 3), 4); return [before, r]; } "
	       "return outer(1);", "[ 1, 1334 ]");
	// a call in the middle of an expression, an array and an object literal
	Expect("fn id(x) { return x; } const o = { a: id(2), b: [id(3), id(id(4))] }; return [id(1), o.a, o.b, 1 + id(5) * id(6)];",
	       "[ 1, 2, [ 3, 4 ], 31 ]");
	// a function without a return gives undefined, also when it ends in the middle of a loop body
	Expect("fn f() { for (const x : [1, 2]) { let y = x; } } fn g(n) { while (true) { if (n > 2) { return n; } n++; } } return [f(), g(0)];",
	       "[ undefined, 3 ]");
}

TEST_CASE("calls | exceptions unwind several frames to the closest handler") {
	// thrown three calls deep, caught at the top: the code after the catch runs in the right frame
	Expect("fn c() { throw \"deep\"; } fn b() { c(); return \"not here\"; } fn a() { b(); return \"nor here\"; } "
	       "let r = \"\"; try { a(); } catch (e) { r = e; } return r + \"!\";", "deep!");
	// the middle frame catches, and its caller gets the return value as usual
	Expect("fn c() { throw 1; } fn b() { try { c(); } catch (e) { return e + 10; } return 0; } fn a() { return b() + 100; } "
	       "return a();", "111");
	// a handler in a frame further down, with a loop and locals of its own
	Expect("fn thrower(n) { if (n === 3) { throw n; } return n; } "
	       "fn run() { const seen = []; for (let i = 0; i < 5; i++) { try { std.array.push(seen, thrower(i)); } catch (e) { std.array.push(seen, -e); } } return seen; } "
	       "return run();", "[ 0, 1, 2, -3, 4 ]");
	// rethrowing from a catch, through recursion
	Expect("fn f(n) { if (n === 0) { throw 9; } try { return f(n - 1); } catch (e) { throw e * 10 + n; } } "
	       "try { f(3); } catch (e) { return e; }", "9123");
	// closures created in the frames that were unwound keep their values
	Expect("let fs = []; fn f(n) { const v = n * 2; std.array.push(fs, () => v); if (n === 2) { throw n; } f(n + 1); } "
	       "try { f(0); } catch (e) { fs = fs; } return std.array.transform(fs, (g) => g());", "[ 0, 2, 4 ]");
}

TEST_CASE("calls | uncaught exceptions from deep calls end the script") {
	ExpectError("fn c() { throw \"oops\"; } fn b() { c(); } fn a() { b(); } a(); return 1;", "uncaught exception: oops");
}

TEST_CASE("calls | natives calling script functions") {
	// the callback runs in a nested loop that stops when the callback returns
	Expect("fn depth(n) { return n === 0 ? 0 : depth(n - 1) + 1; } return std.array.transform([10, 500, 1000], (n) => depth(n));",
	       "[ 10, 500, 1000 ]");
	// callbacks calling natives calling callbacks
	Expect("return std.array.transform([1, 2], (x) => std.array.transform([10, 20], (y) => x * y));", "[ [ 10, 20 ], [ 20, 40 ] ]");
	// an exception inside a callback can be caught inside it
	Expect("return std.array.transform([1, 2, 3], (x) => { try { if (x === 2) { throw \"two\"; } return x; } catch (e) { return e; } });",
	       "[ 1, two, 3 ]");
	// but it can't unwind through the native
	ExpectError("try { std.array.transform([1], (x) => { throw \"out\"; }); } catch (e) { return e; }",
	            "uncaught exception in a function called by a native function: out");
	// the frames of the native's caller are still intact afterwards
	Expect("fn sum(arr) { let s = 0; for (const x : arr) { s += x; } return s; } "
	       "fn run() { const a = 1; const r = std.array.transform([[1, 2], [3]], (xs) => sum(xs)); return [a, r]; } return run();",
	       "[ 1, [ 3, 3 ] ]");
}

TEST_CASE("calls | recursing through natives is still limited by the native stack") {
	// every level goes through std.array.transform, which calls back into the script from C++
	// (which limit comes first depends on the build: frames are cheap in release, native frames big in debug)
	const auto result = RunScript("fn f(n) { return std.array.transform([n], (x) => f(x + 1)); } return f(0);");
	INFO(result.output);
	REQUIRE(result.value.has_value());
	REQUIRE(result.value->type == Value::Type::t_undefined);
	const auto limited = result.output.find("stack overflow: native functions calling script functions go too deep") != std::string::npos
		|| result.output.find("exceeded 1024 call frames") != std::string::npos;
	REQUIRE(limited);
}
