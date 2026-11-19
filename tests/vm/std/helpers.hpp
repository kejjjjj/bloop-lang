#pragma once

#include "tests/vm/defs.hpp"

#include <optional>
#include <string>

// small inline scripts whose result is compared through its printed form (std.string.from / console.log)
namespace bloop::test::std_lib {

	struct Result {
		std::optional<Value> value;
		std::string output;
	};

	inline Result RunScript(const std::string& src) {
		Result result;
		result.value = bloop::test::TEST_ExecuteBuffer(src);
		result.output = bloop::test::TEST_LastOutput();
		return result;
	}

	// the script's return value printed as a string
	inline void Expect(const std::string& src, const std::string& expected) {
		const auto result = RunScript(src);
		INFO("script: " << src);
		INFO("output: " << result.output);
		REQUIRE(result.value.has_value());
		REQUIRE(result.value->ValueToString() == expected);
	}

	// the script terminates with an error that contains `message`
	inline void ExpectError(const std::string& src, const std::string& message) {
		const auto result = RunScript(src);
		INFO("script: " << src);
		INFO("output: " << result.output);
		REQUIRE(result.value.has_value());
		REQUIRE(result.value->type == Value::Type::t_undefined);
		REQUIRE(result.output.find("runtime error") != std::string::npos);
		REQUIRE(result.output.find(message) != std::string::npos);
	}
}
