#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

// `text ${expression} text`

TEST_CASE("template strings | substitutions") {
	Expect("const name = \"guard\"; const hp = 42; return `hello ${name}, hp: ${hp}!`;", "hello guard, hp: 42!");
	// at the start and the end, next to each other, only one
	Expect("const a = 1; const b = 2; return [`${a} and ${b}`, `${a}${b}`, `${a}`];", "[ 1 and 2, 12, 1 ]");
	// any expression
	Expect("fn twice(x) { return x * 2; } const arr = [10, 20]; "
	       "return `${1 + 2 * 3} ${twice(5)} ${arr[1]} ${arr[0] > 5 ? \"big\" : \"small\"} ${std.length(arr)}`;",
	       "7 10 20 big 2");
	// a comma is the sequence operator, like in parentheses
	Expect("return `${1, 2}`;", "2");
	// the result is an ordinary string
	Expect("const s = `a${1}b`; return [s === \"a1b\", std.length(s), std.string.to_upper(s)];", "[ true, 3, A1B ]");
	// used as a statement (the value is thrown away)
	Expect("let n = 0; `${n++}`; return n;", "1");
}

TEST_CASE("template strings | values become text like std.string.from") {
	Expect("return `${true} ${undefined} ${-3} ${[1, 2]}`;", "true undefined -3 [ 1, 2 ]");
	Expect("const d = 2.5; return `${d}` === std.string.from(d);", "true");
	Expect("const o = { a: 1 }; return `${o}` === std.string.from(o);", "true");
	Expect("fn f() { return 1; } return `${f}`;", "function");
}

TEST_CASE("template strings | nesting and braces") {
	Expect("const name = \"x\"; return `outer ${`inner ${name}`} done`;", "outer inner x done");
	Expect("return `${`${`${1}`}`}`;", "1");
	// { } inside a substitution don't end it: objects, lambdas with a body, nested templates with their own substitutions
	Expect("return `${{ a: 1, b: { c: 2 } }.b.c}`;", "2");
	Expect("return `value: ${(() => { const x = { k: 5 }; return x.k; })()}!`;", "value: 5!");
	Expect("const items = [\"a\", \"b\"]; return `${std.array.join(std.array.transform(items, (s) => `<${s}>`), \"\")}`;", "<a><b>");
}

TEST_CASE("template strings | text, escapes and lines") {
	Expect("return [``, `plain`, `${\"\"}`];", "[ , plain,  ]");
	// \` and \$ are text, and so is a $ that isn't followed by {
	Expect("return `\\` \\${not} $5 { } $ {x}`;", "` ${not} $5 { } $ {x}");
	Expect("return `tab\\there`;", "tab\there");
	// they can span lines (a \r\n is a \n)
	Expect("return `one\ntwo ${1 + 1}\nthree`;", "one\ntwo 2\nthree");
	Expect("return `a\r\nb`;", "a\nb");
	// lines after a multi line template keep their numbers in error messages
	ExpectError("const s = `a\nb\nc`;\nconst arr = [1];\nreturn arr[5];", "at [5, ");
}

TEST_CASE("template strings | in functions, loops and the gc") {
	Expect("fn label(i) { return `#${i}`; } let out = []; for (let i = 0; i < 3; i++) { std.array.push(out, label(i)); } "
	       "return std.array.join(out, \",\");", "#0,#1,#2");
	Expect("let last = \"\"; for (let i = 0; i < 2000; i++) { const big = [i, `${i}`]; last = `${big[1]}-${i * 2}`; } "
	       "std.gc_major(); return last;", "1999-3998");
}

TEST_CASE("template strings | errors") {
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return `never ends;"), Catch::Matchers::ContainsSubstring("the template string doesn't end"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return `a ${1 + 2`;"), Catch::Matchers::ContainsSubstring("the template string doesn't end"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return `a ${1 + 2;"), Catch::Matchers::ContainsSubstring("isn't closed with }"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("return `a ${} b`;"), Catch::Matchers::ContainsSubstring("needs an expression"));
	REQUIRE_THROWS(bloop::test::TEST_CompileBuffer("return `a ${1 +} b`;"));
}
