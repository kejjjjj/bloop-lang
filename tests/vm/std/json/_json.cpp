#include "tests/vm/std/helpers.hpp"

#include <string>

using namespace bloop::test::std_lib;

namespace {
	// the script's return value converted back to json
	void ExpectJson(const std::string& src, const std::string& expected) {
		Expect("return std.json.stringify(" + src + ");", expected);
	}
	// std.json.stringify(std.json.parse(text))
	void ExpectRoundTrip(const std::string& text, const std::string& expected) {
		const auto result = RunScript("return std.json.parse(" + text + ");");
		INFO("json: " << text);
		INFO("output: " << result.output);
		REQUIRE(result.value.has_value());
		Expect("return std.json.stringify(std.json.parse(" + text + "));", expected);
	}
	void ExpectInvalid(const std::string& text) {
		Expect("return std.json.parse(" + text + ") === undefined;", "true");
	}
}

TEST_CASE("std.json.stringify primitives") {
	ExpectJson("undefined", "null");
	ExpectJson("true", "true");
	ExpectJson("false", "false");
	ExpectJson("42", "42");
	ExpectJson("-7", "-7");
	ExpectJson("1.5", "1.5");
	ExpectJson("0.1", "0.1");
	// plain digits while they're reasonable, the exponent form only past that
	ExpectJson("10.0", "10");
	ExpectJson("[100.0, 2500.0, 0.001, 100000000000000000000.0, 0.000001]", "[100,2500,0.001,1e+20,1e-06]");
	ExpectJson("1.0 / 3.0", "0.3333333333333333");
	ExpectJson("\"text\"", "\"text\"");
}
TEST_CASE("std.json.stringify escapes strings") {
	ExpectJson("\"quote \\\" backslash \\\\ newline \\n tab \\t\"", "\"quote \\\" backslash \\\\ newline \\n tab \\t\"");
	ExpectJson("std.string.from_char_code(1)", "\"\\u0001\"");
	ExpectJson("\"\xC3\xA4\"", "\"\xC3\xA4\""); // utf-8 stays as it is
}
TEST_CASE("std.json.stringify arrays and objects") {
	ExpectJson("[1, \"a\", [true, undefined]]", "[1,\"a\",[true,null]]");
	ExpectJson("{ b: 1, a: { c: [] }, d: {} }", "{\"b\":1,\"a\":{\"c\":[]},\"d\":{}}"); // insertion order
	ExpectJson("[]", "[]");
	ExpectJson("{}", "{}");
}
TEST_CASE("std.json.stringify with an indent") {
	Expect("return std.json.stringify({ a: [1, 2], b: {} }, 2);", "{\n  \"a\": [\n    1,\n    2\n  ],\n  \"b\": {}\n}");
	Expect("return std.json.stringify([1], 0);", "[1]");
	Expect("return std.json.stringify([[]], 4);", "[\n    []\n]");
}
TEST_CASE("std.json.stringify errors") {
	ExpectError("std.json.stringify(() => 1);", "can't be converted to json");
	ExpectError("std.json.stringify({ f: std.json.parse });", "can't be converted to json");
	ExpectError("let a = []; std.array.push(a, a); std.json.stringify(a);", "contains itself");
	ExpectError("let o = { inner: {} }; std.object.set(o.inner, \"parent\", o); std.json.stringify(o);", "contains itself");
	ExpectError("std.json.stringify(std.json.parse(\"1e999\"));", "NaN and infinity");
	ExpectError("std.json.stringify(1, -1);", "indent");
	ExpectError("std.json.stringify(1, 17);", "indent");
	// the same array twice isn't a cycle
	Expect("const a = [1]; return std.json.stringify([a, a]);", "[[1],[1]]");
}
TEST_CASE("std.json.parse values") {
	ExpectRoundTrip("\" { \\\"a\\\" : [1, -2.5, \\\"x\\\", null, true, false], \\\"b\\\": {} } \"", "{\"a\":[1,-2.5,\"x\",null,true,false],\"b\":{}}");
	ExpectRoundTrip("\"[]\"", "[]");
	ExpectRoundTrip("\"\\\"\\\\u00e4\\\\n\\\\/\\\"\"", "\"\xC3\xA4\\n/\"");
	ExpectRoundTrip("\"{\\\"a\\\":1,\\\"a\\\":2}\"", "{\"a\":2}"); // the last duplicate wins
	ExpectRoundTrip("\"{\\\"z\\\":1,\\\"y\\\":2,\\\"x\\\":3}\"", "{\"z\":1,\"y\":2,\"x\":3}"); // order is kept
	Expect("return std.json.parse(\"null\") === undefined;", "true");
	Expect("return std.length(std.json.parse(\"\\\"\\\\ud83d\\\\ude00\\\"\"));", "4"); // surrogate pair -> 4 utf-8 bytes
}
TEST_CASE("std.json.parse number types") {
	const auto check = [](const std::string& json, Value::Type type) {
		const auto ret = bloop::test::TEST_ExecuteBuffer("return std.json.parse(\"" + json + "\");");
		INFO(json);
		REQUIRE(ret.has_value());
		REQUIRE(ret->type == type);
	};
	check("5", Value::Type::t_int);
	check("-0", Value::Type::t_int);
	check("5.0", Value::Type::t_double);
	check("5e2", Value::Type::t_double);
	check("9223372036854775807", Value::Type::t_int);
	check("9223372036854775808", Value::Type::t_double); // too big for an int
	Expect("return std.json.parse(\"9223372036854775807\");", "9223372036854775807");
	Expect("return std.json.parse(\"5e2\") === 500.0;", "true");
}
TEST_CASE("std.json.parse returns undefined for invalid json") {
	for (const auto* text : {
		"\"\"", "\" \"", "\"{\"", "\"[1,]\"", "\"{\\\"a\\\":1,}\"", "\"{'a':1}\"", "\"{a:1}\"", "\"01\"", "\"1.\"", "\".5\"", "\"-\"",
		"\"1e\"", "\"+1\"", "\"nul\"", "\"truee\"", "\"[1] x\"", "\"\\\"abc\"", "\"\\\"\\\\x\\\"\"", "\"\\\"\\\\u12\\\"\"",
		"\"\\\"\\\\udc00\\\"\"", "\"\\\"\\\\ud800\\\"\"", "\"// comment\\n1\"", "\"NaN\"", "\"[1 2]\"", "\"{\\\"a\\\" 1}\"" }) {
		INFO(text);
		ExpectInvalid(text);
	}
	// a raw newline inside a string
	ExpectInvalid("\"\\\"a\\nb\\\"\"");
}
TEST_CASE("std.json.parse rejects very deep nesting") {
	ExpectInvalid("\"" + std::string(600, '[') + std::string(600, ']') + "\"");
	ExpectRoundTrip("\"" + std::string(100, '[') + std::string(100, ']') + "\"", std::string(100, '[') + std::string(100, ']'));
	ExpectError("let a = []; for (let i = 0; i < 600; i++) { a = [a]; } std.json.stringify(a);", "nested more than");
}
TEST_CASE("std.json survives garbage collections") {
	// thousands of strings, arrays and objects allocated while the result is being built
	Expect(R"(
		let items = [];
		for (let i = 0; i < 2000; i++)
			std.array.push(items, { id: i, name: "item" + std.string.from(i), tags: ["a", "b"], nested: { value: i * 2 } });

		const text = std.json.stringify(items);
		const parsed = std.json.parse(text);
		return [std.length(parsed), parsed[1999].name, parsed[1234].nested.value, std.json.stringify(parsed) === text];
	)", "[ 2000, item1999, 2468, true ]");
}
