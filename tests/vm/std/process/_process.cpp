#include "tests/vm/std/helpers.hpp"

#include <string>

using namespace bloop::test::std_lib;

namespace {
	struct ExitResult {
		std::optional<Value> value;
		std::optional<bloop::BloopInt> exitCode;
		std::string output;
	};
	ExitResult Run(const std::string& src) {
		ExitResult result;
		result.value = bloop::test::TEST_ExecuteBuffer(src);
		result.exitCode = bloop::test::TEST_GetLastVM()->GetExitCode();
		result.output = bloop::test::TEST_LastOutput();
		return result;
	}
}

TEST_CASE("std.process.args") {
	auto environment = bloop::standard::Environment::Unrestricted();
	environment.m_oArguments = { "first", "second arg" };

	const auto ret = bloop::test::TEST_ExecuteBuffer("return std.json.stringify(std.process.args());", environment);
	REQUIRE(ret.has_value());
	REQUIRE(ret->ValueToString() == "[\"first\",\"second arg\"]");

	Expect("return std.length(std.process.args());", "0");
	// every call is a new array
	Expect("let a = std.process.args(); std.array.push(a, 1); return std.length(std.process.args());", "0");
}
TEST_CASE("std.process.env") {
	Expect("return std.process.env(\"BLOOP_TEST_SURELY_NOT_SET_9271\") === undefined;", "true");
	Expect("const path = std.process.env(\"PATH\"); return std.length(path) > 0;", "true");
	ExpectError("std.process.env(\"\");", "invalid variable name");
	ExpectError("std.process.env(\"A=B\");", "invalid variable name");
}
TEST_CASE("std.process.platform") {
	const auto ret = bloop::test::TEST_ExecuteBuffer("return std.process.platform;");
	REQUIRE(ret.has_value());
	const auto platform = ret->ValueToString();
	REQUIRE((platform == "windows" || platform == "macos" || platform == "linux"));
}
TEST_CASE("std.process.exit ends the script") {
	{
		const auto result = Run("std.process.exit(3);\nreturn 5;");
		REQUIRE(result.value.has_value());
		REQUIRE(result.value->type == Value::Type::t_undefined);
		REQUIRE(result.exitCode == 3);
		REQUIRE(result.output.empty()); // it's not an error
		REQUIRE_FALSE(bloop::test::TEST_GetLastVM()->Failed());
	}
	REQUIRE(Run("std.process.exit();").exitCode == 0);
	REQUIRE(Run("return 1;").exitCode == std::nullopt);
}
TEST_CASE("std.process.exit from deep inside the script") {
	// nested calls
	REQUIRE(Run("fn a(n) { if (n === 0) { std.process.exit(4); } return a(n - 1); } a(50); return 1;").exitCode == 4);
	// a callback that a native function called
	REQUIRE(Run("std.array.for_each([1, 2, 3], (x) => { if (x === 2) { std.process.exit(x); } }); return 1;").exitCode == 2);
	// try/catch can't stop it
	REQUIRE(Run("try { std.process.exit(6); } catch (e) { return \"caught\"; } return 1;").exitCode == 6);
	// closures with open upvalues get closed on the way out
	REQUIRE(Run("fn f() { let x = 1; const g = () => x; std.process.exit(9); return g; } f();").exitCode == 9);
}
TEST_CASE("runtime errors mark the VM as failed") {
	const auto result = Run("return [1][5];");
	REQUIRE(bloop::test::TEST_GetLastVM()->Failed());
	REQUIRE(result.exitCode == std::nullopt);
}
