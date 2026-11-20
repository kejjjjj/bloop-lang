#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// std.coroutine: create, resume, yield, status, running

#define CO "const create = std.coroutine.create; const resume = std.coroutine.resume; const yield = std.coroutine.yield; " \
           "const status = std.coroutine.status; "

TEST_CASE("std.coroutine | values go both ways") {
	Expect(CO "const co = create((x) => { const y = yield(x + 1); return y * 2; }); "
	       "const a = resume(co, 1); const s = status(co); const b = resume(co, 10); return [a, s, b, status(co)];",
	       "[ 2, suspended, 20, dead ]");
	// no value either way is undefined
	Expect(CO "const co = create(() => { const got = yield(); return got; }); return [resume(co), resume(co)];",
	       "[ undefined, undefined ]");
	// a function without a return
	Expect(CO "const co = create(() => { yield(1); }); return [resume(co), resume(co), status(co)];", "[ 1, undefined, dead ]");
	// named functions too, and several parameters
	Expect(CO "fn add(a, b, c) { yield(a + b); return a + b + c; } const co = create(add); return [resume(co, 1, 2, 3), resume(co)];",
	       "[ 3, 6 ]");
}

TEST_CASE("std.coroutine | generators") {
	Expect(CO "fn squares(n) { for (let i = 0; i < n; i++) { yield(i * i); } return \"done\"; } "
	       "const g = create(squares); const out = [resume(g, 4)]; "
	       "while (status(g) != \"dead\") { std.array.push(out, resume(g)); } return out;",
	       "[ 0, 1, 4, 9, done ]");
	// a ranged for over an array keeps its iterator on the stack while suspended
	Expect(CO "const g = create((arr) => { for (const x : arr) { yield(x); } }); "
	       "const out = []; let v = resume(g, [\"a\", \"b\", \"c\"]); "
	       "while (status(g) != \"dead\") { std.array.push(out, v); v = resume(g); } return out;",
	       "[ a, b, c ]");
	// two generators at once
	Expect(CO "fn count(from) { while (true) { yield(from); from++; } } "
	       "const a = create(count); const b = create(count); "
	       "return [resume(a, 0), resume(b, 100), resume(a), resume(a), resume(b)];",
	       "[ 0, 100, 1, 2, 101 ]");
}

TEST_CASE("std.coroutine | yield from any depth of calls") {
	// the reason for coroutines: wait() is an ordinary function
	Expect(CO "fn wait(seconds) { return yield(seconds); } "
	       "fn helper(x) { const got = wait(x); return got + 1; } "
	       "const co = create(() => { const a = helper(5); const b = wait(a); return [a, b]; }); "
	       "return [resume(co), resume(co, 41), resume(co, \"last\")];",
	       "[ 5, 42, [ 42, last ] ]");
	// from inside recursion
	Expect(CO "fn walk(n) { if (n === 0) { return 0; } yield(n); return walk(n - 1) + n; } "
	       "const co = create(walk); const out = [resume(co, 3)]; "
	       "while (status(co) != \"dead\") { std.array.push(out, resume(co)); } return out;",
	       "[ 3, 2, 1, 6 ]");
	// resumed from different depths: the coroutine's frames move to wherever the stack is
	Expect(CO "const co = create(() => { let total = 0; while (true) { total += yield(total); } }); "
	       "fn deep(n, v) { return n === 0 ? resume(co, v) : deep(n - 1, v); } "
	       "resume(co); const a = deep(20, 1); const b = resume(co, 10); const c = deep(5, 100); return [a, b, c];",
	       "[ 1, 11, 111 ]");
}

TEST_CASE("std.coroutine | status and running") {
	Expect(CO "let inside = \"\"; const co = create(() => { inside = status(std.coroutine.running()); yield(0); }); "
	       "const before = status(co); resume(co); return [before, inside, status(co), std.coroutine.running()];",
	       "[ suspended, running, suspended, undefined ]");
	// the resumer is "normal" while the coroutine it resumed runs
	Expect(CO "let outer = undefined; let seen = \"\"; "
	       "const inner = create(() => { seen = status(outer); yield(1); }); "
	       "outer = create(() => { resume(inner); return status(inner); }); "
	       "return [resume(outer), seen, status(outer)];",
	       "[ suspended, normal, dead ]");
}

TEST_CASE("std.coroutine | nested coroutines") {
	// a coroutine resumes another one, which yields back to it, and then yields to the main script
	Expect(CO "const inner = create(() => { yield(\"inner 1\"); yield(\"inner 2\"); return \"inner done\"; }); "
	       "const outer = create(() => { const log = []; "
	       "  while (status(inner) != \"dead\") { std.array.push(log, resume(inner)); yield(std.array.join(log, \",\")); } "
	       "  return \"outer done\"; }); "
	       "return [resume(outer), resume(outer), resume(outer), resume(outer)];",
	       "[ inner 1, inner 1,inner 2, inner 1,inner 2,inner done, outer done ]");
	// a coroutine that creates coroutines
	Expect(CO "fn range(n) { return create(() => { for (let i = 0; i < n; i++) { yield(i); } }); } "
	       "const sums = create(() => { for (let n = 1; n <= 3; n++) { const r = range(n); let s = 0; let v = resume(r); "
	       "  while (status(r) != \"dead\") { s += v; v = resume(r); } yield(s); } }); "
	       "return [resume(sums), resume(sums), resume(sums)];",
	       "[ 0, 1, 3 ]");
}

TEST_CASE("std.coroutine | closures share variables with a suspended coroutine") {
	// the coroutine's local, captured by a closure that escapes while the coroutine is paused
	Expect(CO "const co = create(() => { let x = 1; yield(() => x); x = 5; yield(0); x = 7; }); "
	       "const get = resume(co); const a = get(); resume(co); const b = get(); resume(co); const c = get(); "
	       "return [a, b, c, status(co)];",
	       "[ 1, 5, 7, dead ]");
	// and writes from outside are seen inside
	Expect(CO "const co = create(() => { let x = 0; yield((v) => { x = v; }); return x; }); "
	       "const set = resume(co); set(42); return resume(co);",
	       "42");
	// several captured variables, in several frames of the coroutine
	Expect(CO "fn inner(a) { let b = a * 10; yield(() => a + b); b = 1000; yield(0); } "
	       "const co = create((n) => { let c = n; const f = create; inner(c); return c; }); "
	       "const get = resume(co, 2); const before = get(); resume(co); const after = get(); return [before, after, resume(co)];",
	       "[ 22, 1002, 2 ]");
	// the coroutine uses a variable of the function that created it
	Expect(CO "fn make() { let count = 0; const co = create(() => { while (true) { count++; yield(count); } }); "
	       "return [co, () => count]; } "
	       "const pair = make(); resume(pair[0]); resume(pair[0]); return [resume(pair[0]), pair[1]()];",
	       "[ 3, 3 ]");
}

TEST_CASE("std.coroutine | exceptions") {
	// caught inside the coroutine
	Expect(CO "const co = create(() => { try { throw \"x\"; } catch (e) { yield(e); } return \"after\"; }); "
	       "return [resume(co), resume(co)];",
	       "[ x, after ]");
	// a try around a yield: the handler survives the suspension
	Expect(CO "const co = create(() => { try { yield(1); throw \"later\"; } catch (e) { return \"caught \" + e; } }); "
	       "const a = resume(co); fn deeper() { return resume(co); } return [a, deeper()];",
	       "[ 1, caught later ]");
	// not caught inside: it ends the coroutine and gets thrown where it was resumed
	Expect(CO "const co = create(() => { yield(1); throw \"boom\"; }); resume(co); "
	       "let got = \"\"; try { resume(co); } catch (e) { got = e; } return [got, status(co)];",
	       "[ boom, dead ]");
	// through several frames on both sides
	Expect(CO "fn fail() { throw 7; } fn work() { yield(0); fail(); } const co = create(work); "
	       "fn step() { return resume(co); } fn run() { try { step(); step(); } catch (e) { return e * 6; } } return run();",
	       "42");
	// through nested coroutines
	Expect(CO "const inner = create(() => { throw \"from inner\"; }); "
	       "const outer = create(() => { try { resume(inner); } catch (e) { yield(\"outer caught \" + e); } }); "
	       "return [resume(outer), status(inner)];",
	       "[ outer caught from inner, dead ]");
	// not caught anywhere
	ExpectError(CO "const co = create(() => { throw \"nobody\"; }); resume(co); return 1;", "uncaught exception: nobody");
}

TEST_CASE("std.coroutine | misuse is a runtime error") {
	ExpectError(CO "const co = create(() => 1); resume(co); resume(co);", "std.coroutine.resume: the coroutine is dead");
	ExpectError(CO "let co = undefined; co = create(() => { resume(co); }); resume(co);", "std.coroutine.resume: the coroutine is already running");
	ExpectError(CO "yield(1);", "std.coroutine.yield: not inside a coroutine");
	ExpectError(CO "fn f() { yield(1); } f();", "std.coroutine.yield: not inside a coroutine");
	// (resuming from inside a callback is fine: no native is between that resume and the yield)
	Expect(CO "const cos = [create(() => { yield(1); return 2; }), create(() => { yield(10); return 20; })]; "
	       "const a = std.array.transform(cos, (co) => resume(co)); const b = std.array.transform(cos, (co) => resume(co)); "
	       "return [a, b];", "[ [ 1, 10 ], [ 2, 20 ] ]");
	// a native's C++ frames can't be paused
	ExpectError(CO "const co = create(() => { std.array.transform([1, 2], (x) => yield(x)); }); resume(co);",
	            "can't yield inside a function that a native function called");
	ExpectError(CO "const co = create((a, b) => a); resume(co, 1);", "passed 1 arguments, but the coroutine's function expects 2");
	ExpectError(CO "const co = create(() => yield(1)); resume(co); resume(co, 1, 2);", "a coroutine that has started takes at most 1");
	ExpectError(CO "create(5);", "std.coroutine.create: argument 1 must be a script function");
	ExpectError(CO "create(std.array.push);", "std.coroutine.create: argument 1 must be a script function");
	ExpectError(CO "resume([]);", "std.coroutine.resume: argument 1 must be \"coroutine\"");
	ExpectError(CO "status(1);", "std.coroutine.status: argument 1 must be \"coroutine\"");
	// a runtime error inside a coroutine ends the whole script, like anywhere else
	ExpectError(CO "const co = create(() => { yield(0); return [1][5]; }); resume(co); resume(co);", "out of bounds index [5]");
}

TEST_CASE("std.coroutine | coroutines are values") {
	Expect(CO "const co = create(() => 1); return [std.string.from(co), co == co, co == create(() => 1)];",
	       "[ coroutine, true, false ]");
	ExpectError(CO "return std.json.stringify(create(() => 1));", "a value of type \"coroutine\" can't be converted to json");
}

TEST_CASE("std.coroutine | the frame and stack limits count the coroutine's frames too") {
	Expect(CO "fn depth(n) { return n === 0 ? yield(\"bottom\") : depth(n - 1); } const co = create(depth); "
	       "return resume(co, 900);", "bottom");
	ExpectError(CO "fn f(n) { return f(n + 1); } const co = create(f); resume(co, 0);", "exceeded 1024 call frames");
	// resuming deep inside recursion
	ExpectError(CO "const co = create(() => { fn g(n) { return n === 0 ? 0 : g(n - 1); } yield(0); return g(600); }); "
	            "resume(co); fn deep(n) { return n === 0 ? resume(co) : deep(n - 1); } deep(600);",
	            "exceeded 1024 call frames");
}

TEST_CASE("std.coroutine | the garbage collector") {
	// young values held only by a suspended coroutine, across collections
	Expect(CO "const co = create(() => { const keep = [\"a\", [\"b\"], { c: \"c\" }]; yield(0); std.gc_minor(); yield(0); "
	       "return keep[0] + keep[1][0] + keep[2].c; }); "
	       "resume(co); std.gc_minor(); std.gc_minor(); std.gc_minor(); resume(co); std.gc_major(); return resume(co);",
	       "abc");
	// many coroutines, each holding its own objects
	Expect(CO "const cos = []; for (let i = 0; i < 200; i++) { "
	       "  std.array.push(cos, create((n) => { let s = [n]; for (let k = 0; k < 3; k++) { s = [s[0] + 1]; yield(s[0]); } return s[0] * 1000; })); } "
	       "let total = 0; for (let round = 0; round < 4; round++) { for (let i = 0; i < 200; i++) { "
	       "  total += round === 0 ? resume(cos[i], i) : resume(cos[i]); } std.gc_minor(); } return total;",
	       // sum over i of (i+1) + (i+2) + (i+3) + (i+3)*1000
	       std::to_string(3 * 19900 + 6 * 200 + 1000 * (19900 + 3 * 200)));
	// a closure into an abandoned coroutine keeps it alive
	Expect(CO "fn make() { const co = create(() => { let secret = [\"kept\"]; yield(() => secret[0]); }); return resume(co); } "
	       "const get = make(); std.gc_major(); std.gc_minor(); std.gc_major(); return get();",
	       "kept");
}

TEST_CASE("std.coroutine | dead and unreachable coroutines are freed") {
	// (in a function: variables at the top level are globals, which would keep the last one alive)
	const auto result = RunScript(CO "fn run() { for (let i = 0; i < 300; i++) { const co = create(() => { yield([i]); }); resume(co); } } "
	                              "run(); std.gc_major(); return 1;");
	REQUIRE(result.value.has_value());
	REQUIRE(bloop::test::TEST_GetLastVM()->GetGC().CoroutineCount() == 0u);

	const auto kept = RunScript(CO "const keep = []; for (let i = 0; i < 10; i++) { const co = create(() => { yield(0); }); "
	                            "resume(co); std.array.push(keep, co); } std.gc_major(); return std.length(keep);");
	REQUIRE(kept.value.has_value());
	REQUIRE(bloop::test::TEST_GetLastVM()->GetGC().CoroutineCount() == 10u);
}

TEST_CASE("std.coroutine | a scheduler built on top") {
	// what a game would ship: wait() is plain bloop, the language knows nothing about time
	Expect(CO
	       "const tasks = []; let now = 0; const log = []; "
	       "fn spawn(f) { std.array.push(tasks, { co: create(f), until: 0 }); } "
	       "fn wait(seconds) { yield(seconds); } "
	       "fn tick(dt) { now += dt; for (const t : tasks) { "
	       "  if (status(t.co) != \"dead\" && now >= t.until) { const s = resume(t.co); t.until = now + (s === undefined ? 0 : s); } } } "
	       "spawn(() => { std.array.push(log, \"door opens at \" + std.string.from(now)); wait(2); "
	       "              std.array.push(log, \"guard speaks at \" + std.string.from(now)); }); "
	       "spawn(() => { for (let i = 0; i < 3; i++) { std.array.push(log, \"tick task \" + std.string.from(i)); wait(1); } }); "
	       "for (let frame = 0; frame < 6; frame++) { tick(1); } "
	       "return std.array.join(log, \" | \");",
	       "door opens at 1 | tick task 0 | tick task 1 | guard speaks at 3 | tick task 2");
}
