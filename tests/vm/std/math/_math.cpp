#include "tests/vm/std/helpers.hpp"

#include <cmath>
#include <string>

using namespace bloop::test::std_lib;

// std.math

namespace {
	// a double result, compared with a tolerance (doubles print with 6 decimals)
	void ExpectNear(const std::string& expression, double expected, double tolerance = 1e-9) {
		const auto ret = bloop::test::TEST_ExecuteBuffer("return " + expression + ";");
		INFO(expression << "\n" << bloop::test::TEST_LastOutput());
		REQUIRE(ret.has_value());
		REQUIRE(ret->type == Value::Type::t_double);
		REQUIRE(std::fabs(static_cast<double>(ret->d) - expected) <= tolerance);
	}
	// the type of the result
	void ExpectType(const std::string& expression, Value::Type type) {
		const auto ret = bloop::test::TEST_ExecuteBuffer("return " + expression + ";");
		INFO(expression << "\n" << bloop::test::TEST_LastOutput());
		REQUIRE(ret.has_value());
		REQUIRE(ret->type == type);
	}
}

TEST_CASE("std.math | constants") {
	ExpectNear("std.math.pi", 3.14159265358979323846);
	ExpectNear("std.math.tau", 2.0 * 3.14159265358979323846);
	ExpectNear("std.math.e", 2.71828182845904523536);
	Expect("return [std.math.inf > 1000000000000000000000.0, -std.math.inf < -1000000000000000000000.0, std.math.is_finite(std.math.inf)];", "[ true, true, false ]");
}

TEST_CASE("std.math | abs, min, max, clamp and sign keep the type") {
	Expect("return [std.math.abs(-5), std.math.abs(5), std.math.abs(0)];", "[ 5, 5, 0 ]");
	ExpectType("std.math.abs(-5)", Value::Type::t_int);
	ExpectNear("std.math.abs(-2.5)", 2.5);
	ExpectError("std.math.abs(-9223372036854775807 - 1);", "std.math.abs: the result doesn't fit in an int");

	// one or more numbers, the result is the argument as it was passed
	Expect("return [std.math.min(3), std.math.min(3, 1, 2), std.math.max(3, 1, 2), std.math.max(-1, -7)];", "[ 3, 1, 3, -1 ]");
	ExpectType("std.math.min(3, 1.5)", Value::Type::t_double);
	ExpectType("std.math.min(1, 1.5)", Value::Type::t_int);
	ExpectNear("std.math.max(1, 2.5, 2)", 2.5);
	// nan wins (like comparing with it would be false either way)
	Expect("return [std.math.is_nan(std.math.min(1, std.math.sqrt(-1))), std.math.is_nan(std.math.max(std.math.sqrt(-1), 1))];",
	       "[ true, true ]");
	// ints are compared exactly, also when they're too big for a double
	Expect("return std.math.max(9007199254740993, 9007199254740992) === 9007199254740993;", "true");

	Expect("return [std.math.clamp(15, 0, 10), std.math.clamp(-3, 0, 10), std.math.clamp(5, 0, 10)];", "[ 10, 0, 5 ]");
	ExpectNear("std.math.clamp(0.5, 0.0, 1.0)", 0.5);
	ExpectNear("std.math.clamp(2, 0.0, 1.0)", 1.0); // the limit that it hit
	ExpectError("std.math.clamp(1, 10, 0);", "std.math.clamp: the low limit must not be higher than the high limit");

	Expect("return [std.math.sign(-3), std.math.sign(0), std.math.sign(42)];", "[ -1, 0, 1 ]");
	ExpectNear("std.math.sign(-0.5)", -1.0);
	ExpectNear("std.math.sign(0.0)", 0.0);

	ExpectError("std.math.abs(\"1\");", "std.math.abs: argument 1 must be \"number\", but got \"string\"");
	ExpectError("std.math.max(1, true);", "std.math.max: argument 2 must be \"number\", but got \"boolean\"");
}

TEST_CASE("std.math | floor, ceil, round and trunc return ints") {
	Expect("return [std.math.floor(2.7), std.math.floor(-2.2), std.math.ceil(2.1), std.math.ceil(-2.7)];", "[ 2, -3, 3, -2 ]");
	// halves round away from zero
	Expect("return [std.math.round(2.5), std.math.round(-2.5), std.math.round(2.4), std.math.round(-0.4)];", "[ 3, -3, 2, 0 ]");
	Expect("return [std.math.trunc(2.7), std.math.trunc(-2.7)];", "[ 2, -2 ]");
	ExpectType("std.math.floor(2.7)", Value::Type::t_int);
	ExpectType("std.math.floor(7)", Value::Type::t_int); // ints stay as they are

	// so they can index arrays
	Expect("const arr = [\"a\", \"b\", \"c\"]; return arr[std.math.floor(2.9)];", "c");

	ExpectError("std.math.floor(std.math.inf);", "std.math.floor: the result doesn't fit in an int");
	ExpectError("std.math.round(std.math.sqrt(-1));", "std.math.round: the result doesn't fit in an int");
	ExpectError("std.math.ceil(10000000000000000000.0);", "std.math.ceil: the result doesn't fit in an int");
}

TEST_CASE("std.math | doubles") {
	ExpectNear("std.math.sqrt(16)", 4.0);
	ExpectNear("std.math.cbrt(-27)", -3.0);
	ExpectNear("std.math.pow(2, 10)", 1024.0);
	ExpectNear("std.math.pow(4, 0.5)", 2.0);
	ExpectNear("std.math.exp(1)", 2.71828182845904523536);
	ExpectNear("std.math.log(std.math.e)", 1.0);
	ExpectNear("std.math.log2(1024)", 10.0);
	ExpectNear("std.math.log10(1000)", 3.0);

	ExpectNear("std.math.sin(std.math.pi / 2)", 1.0);
	ExpectNear("std.math.cos(std.math.pi)", -1.0);
	ExpectNear("std.math.tan(std.math.pi / 4)", 1.0);
	ExpectNear("std.math.asin(1)", 3.14159265358979323846 / 2.0);
	ExpectNear("std.math.acos(1)", 0.0);
	ExpectNear("std.math.atan(1)", 3.14159265358979323846 / 4.0);
	ExpectNear("std.math.atan2(1, -1)", 3.0 * 3.14159265358979323846 / 4.0); // (y, x), all four quadrants
	ExpectNear("std.math.hypot(3, 4)", 5.0);

	ExpectNear("std.math.lerp(10, 20, 0.25)", 12.5);
	ExpectNear("std.math.lerp(10, 20, 1.5)", 25.0); // t isn't clamped
	ExpectNear("std.math.to_radians(180)", 3.14159265358979323846);
	ExpectNear("std.math.to_degrees(std.math.pi / 2)", 90.0);

	// the usual IEEE results, not errors
	Expect("return [std.math.is_nan(std.math.sqrt(-1)), std.math.is_nan(std.math.log(-1)), std.math.log(0) === -std.math.inf];",
	       "[ true, true, true ]");
	Expect("return [std.math.is_nan(1), std.math.is_finite(1), std.math.is_finite(1.5), std.math.is_finite(-std.math.inf)];",
	       "[ false, true, true, false ]");

	ExpectError("std.math.sqrt(\"4\");", "argument 1 must be \"number\"");
}

TEST_CASE("std.math | random numbers") {
	// [0, 1)
	Expect("for (let i = 0; i < 10000; i++) { const r = std.math.random(); if (r < 0 || r >= 1) { return r; } } return \"ok\";", "ok");

	// [low, high], both included, and every value comes up
	Expect("const seen = [false, false, false, false, false, false]; "
	       "for (let i = 0; i < 2000; i++) { const r = std.math.random_int(1, 6); if (r < 1 || r > 6) { return r; } seen[r - 1] = true; } "
	       "return std.array.all(seen, (s) => s);", "true");
	Expect("return [std.math.random_int(5, 5), std.math.random_int(-3, -3)];", "[ 5, -3 ]");
	// the whole int range works too
	ExpectType("std.math.random_int(-9223372036854775807 - 1, 9223372036854775807)", Value::Type::t_int);
	ExpectError("std.math.random_int(6, 1);", "std.math.random_int: the low limit must not be higher than the high limit");
	ExpectError("std.math.random_int(1.5, 6);", "argument 1 must be \"int\"");
}

TEST_CASE("std.math | the same seed gives the same numbers") {
	Expect("std.math.seed(7); const a = [std.math.random(), std.math.random_int(0, 100), std.math.random()]; "
	       "std.math.seed(7); const b = [std.math.random(), std.math.random_int(0, 100), std.math.random()]; "
	       "return a[0] === b[0] && a[1] === b[1] && a[2] === b[2];", "true");

	// on every platform and compiler: these exact numbers (a replay recorded on one machine plays the same on another)
	Expect("std.math.seed(42); return [std.math.random_int(0, 1000000), std.math.random_int(0, 1000000), "
	       "std.math.random_int(-5, 5), std.math.random_int(0, 1000000)];", "[ 341337, 841083, -3, 72568 ]");
	Expect("std.math.seed(-1); return std.math.random_int(0, 1000000);", "76222");

	// the host's seed
	bloop::standard::Environment environment;
	environment.m_oRandomSeed = 42u;
	const auto ret = bloop::test::TEST_ExecuteBuffer("return [std.math.random_int(0, 1000000), std.math.random_int(0, 1000000)];", environment);
	REQUIRE(ret.has_value());
	REQUIRE(ret->ValueToString() == "[ 341337, 841083 ]");

	// without one, two VMs (almost certainly) differ
	const auto first = bloop::test::TEST_ExecuteBuffer("return std.math.random_int(0, 1000000000);");
	REQUIRE(first.has_value());
	const auto firstValue = first->i;
	const auto second = bloop::test::TEST_ExecuteBuffer("return std.math.random_int(0, 1000000000);");
	REQUIRE(second.has_value());
	REQUIRE(firstValue != second->i);
}
