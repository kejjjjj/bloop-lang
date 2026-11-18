#include "tests/vm/defs.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {
	std::pair<std::optional<Value>, std::string> RunCaptured(const std::string& src) {
		std::ostringstream captured;
		auto* old = std::cout.rdbuf(captured.rdbuf());
		auto ret = bloop::test::TEST_ExecuteBuffer(src);
		std::cout.rdbuf(old);
		return { ret, captured.str() };
	}
}

// the last line of a script without a trailing newline wasn't in the line map,
// so reporting an error on it crashed
TEST_CASE("runtime error on the last line without a trailing newline") {
	const auto [ret, output] = RunCaptured("let a = [1];\nreturn a[5];");
	INFO(output);
	REQUIRE(ret.has_value());
	REQUIRE(ret->type == Value::Type::t_undefined);
	REQUIRE(output.find("out of bounds index [5]") != std::string::npos);
	REQUIRE(output.find("at [2, ") != std::string::npos);
	REQUIRE(output.find("return a[5];") != std::string::npos); // the source line is shown
}
TEST_CASE("runtime error in a single line script") {
	const auto [ret, output] = RunCaptured("return [1][5];");
	INFO(output);
	REQUIRE(ret.has_value());
	REQUIRE(output.find("at [1, ") != std::string::npos);
	REQUIRE(output.find("return [1][5];") != std::string::npos);
}
