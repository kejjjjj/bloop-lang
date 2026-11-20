#include "tests/vm/std/helpers.hpp"

#include <chrono>

using namespace bloop::test::std_lib;

// std.time: now, unix, sleep

namespace {
	Value Run(const std::string& src, const bloop::standard::Environment& environment = bloop::standard::Environment::Unrestricted()) {
		const auto ret = bloop::test::TEST_ExecuteBuffer(src, environment);
		INFO(bloop::test::TEST_LastOutput());
		REQUIRE(ret.has_value());
		return *ret;
	}
	double Number(const Value& v) {
		REQUIRE(v.type == Value::Type::t_double);
		return static_cast<double>(v.d);
	}
}

TEST_CASE("std.time.now | seconds since the VM was created, never backwards") {
	const auto start = Number(Run("return std.time.now();"));
	REQUIRE(start >= 0.0);
	REQUIRE(start < 5.0);

	// many reads in a row never go backwards
	Expect("let last = std.time.now(); let ok = true; for (let i = 0; i < 10000; i++) { const t = std.time.now(); "
	       "if (t < last) { ok = false; } last = t; } return ok;", "true");
}

TEST_CASE("std.time.unix | the wall clock") {
	const auto before = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
	const auto wall = Number(Run("return std.time.unix();"));
	const auto after = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

	REQUIRE(wall >= before - 1.0);
	REQUIRE(wall <= after + 1.0);
	REQUIRE(wall > 1.7e9); // (after 2023)
}

TEST_CASE("std.time.sleep | blocks for that long") {
	const auto elapsed = Number(Run("const t = std.time.now(); std.time.sleep(0.05); return std.time.now() - t;"));
	REQUIRE(elapsed >= 0.045);
	REQUIRE(elapsed < 5.0);

	// whole numbers work too
	Expect("std.time.sleep(0); return 1;", "1");

	ExpectError("std.time.sleep(-1);", "std.time.sleep: the time must be a number of seconds that isn't negative");
	ExpectError("std.time.sleep(\"1\");", "argument 1 must be");
}

TEST_CASE("std.time | sleep only exists when the environment allows it") {
	const auto has = [](const std::string& name, const bloop::standard::Environment& environment) {
		return Run("return std.object.has(std.time, \"" + name + "\");", environment).ValueToString() == "true";
	};

	const bloop::standard::Environment locked;
	REQUIRE(has("now", locked));
	REQUIRE(has("unix", locked));
	REQUIRE_FALSE(has("sleep", locked));

	auto withSleep = locked;
	withSleep.m_bSleep = true;
	REQUIRE(has("sleep", withSleep));
	REQUIRE(has("sleep", bloop::standard::Environment::Unrestricted()));
}

TEST_CASE("std.time.now | the host's clock") {
	double gameTime = 12.5;
	bloop::standard::Environment environment;
	environment.m_fnClock = [&gameTime]() { return gameTime; };

	REQUIRE(Number(Run("return std.time.now();", environment)) == 12.5);

	gameTime = 100.0;
	REQUIRE(Number(Run("return std.time.now();", environment)) == 100.0);
}
