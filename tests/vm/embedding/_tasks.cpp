#include "bloop.hpp"
#include "tests/vm/defs.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

// std.task and Script::RunTasks, on a clock that the test moves by hand

using bloop::HostValue;
using bloop::Result;
using bloop::Script;
using bloop::ScriptOptions;

namespace {
	struct Host {
		double clock{};
		std::vector<std::string> lines;

		[[nodiscard]] ScriptOptions Options() {
			ScriptOptions options;
			options.m_oEnvironment.m_fnClock = [this] { return clock; };
			options.m_oEnvironment.m_fnPrint = [this](bloop::BloopStringView line) { lines.emplace_back(line); };
			options.m_sEventsModule = "events";
			return options;
		}

		// loads and runs, failing the test on errors
		[[nodiscard]] Script Start(std::string_view source) {
			Script script(Options());
			const auto loaded = script.LoadSource(source);
			INFO(loaded.m_sError);
			REQUIRE(loaded.Ok());
			const auto ran = script.Run();
			INFO(ran.m_sError);
			REQUIRE(ran.Ok());
			return script;
		}

		// the lines printed since the last call
		[[nodiscard]] std::vector<std::string> Take() { return std::exchange(lines, {}); }
	};

	using Lines = std::vector<std::string>;

	void RunTasks(Script& script, bloop::BloopInt expectedRan) {
		const auto result = script.RunTasks();
		INFO(result.m_sError);
		REQUIRE(result.Ok());
		REQUIRE(result.m_oValue == HostValue(expectedRan));
	}
}

TEST_CASE("std.task | spawn runs until the first wait, RunTasks continues when the time has come") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		const log = std.console.log;
		std.task.spawn((name) => {
			log(name, " 1");
			const waited = std.task.wait(2);
			log(name, " 2 after 2.5: ", waited == 2.5);
			std.task.wait(0.5);
			log(name, " 3");
		}, "a");
		log("main");
	)");

	REQUIRE(host->Take() == Lines{ "a 1", "main" });
	REQUIRE(script.TaskCount() == 1u);
	REQUIRE(script.NextTaskTime() == 2.0);

	host->clock = 1.0;
	RunTasks(script, 0);
	REQUIRE(host->Take().empty());

	host->clock = 2.5;
	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "a 2 after 2.5: true" });
	REQUIRE(script.NextTaskTime() == 3.0);

	host->clock = 3.0;
	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "a 3" });
	REQUIRE(script.TaskCount() == 0u);
	REQUIRE(!script.NextTaskTime());

	RunTasks(script, 0);
}

TEST_CASE("std.task | tasks run in the order they were spawned, and wait() waits for the next RunTasks") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		for (let i = 0; i < 3; i++) {
			std.task.spawn((n) => {
				for (let step = 0; step < 3; step++) {
					std.task.wait();
					std.console.log(n, ":", step);
				}
			}, i);
		}
	)");

	REQUIRE(host->Take().empty());
	RunTasks(script, 3);
	REQUIRE(host->Take() == Lines{ "0:0", "1:0", "2:0" });
	RunTasks(script, 3);
	REQUIRE(host->Take() == Lines{ "0:1", "1:1", "2:1" });
	RunTasks(script, 3);
	REQUIRE(host->Take() == Lines{ "0:2", "1:2", "2:2" });
	REQUIRE(script.TaskCount() == 0u);
}

TEST_CASE("std.task | a task spawned or woken during RunTasks waits for the next one") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		std.task.spawn(() => {
			std.task.wait();
			std.console.log("parent");
			std.task.spawn(() => {
				std.console.log("child starts");
				std.task.wait();
				std.console.log("child continues");
			});
			std.task.wait();
			std.console.log("parent again");
		});
	)");

	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "parent", "child starts" });
	RunTasks(script, 2);
	REQUIRE(host->Take() == Lines{ "parent again", "child continues" });
}

TEST_CASE("std.task | closures keep their variables across waits") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		let total = 0;
		const counters = [];
		for (let i = 0; i < 20; i++) {
			std.task.spawn(() => {
				let mine = 0;
				std.array.push(counters, () => mine);
				while (mine < 5) {
					std.task.wait(1);
					mine++;
					total++;
					std.gc_major();
				}
			});
		}
		export fn total_now() { return total; }
		export fn sum() { return std.array.accumulate(counters, (s, f) => s + f(), 0); }
	)");

	for (auto second = 1; second <= 5; second++) {
		host->clock = second;
		RunTasks(script, 20);
		REQUIRE(script.Call("total_now").m_oValue == HostValue(second * 20));
		REQUIRE(script.Call("sum").m_oValue == HostValue(second * 20));
	}
	REQUIRE(script.TaskCount() == 0u);
}

TEST_CASE("std.task | cancel, alive and current") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		const a = std.task.spawn(() => { std.task.wait(1); std.console.log("a never"); });
		const b = std.task.spawn(() => {
			std.console.log("b is ", std.task.current());
			std.task.cancel(std.task.current());
			std.console.log("b goes on until it waits");
			std.task.wait();
			std.console.log("b never");
		});
		std.console.log(std.task.alive(a), " ", std.task.alive(b));
		std.console.log(std.task.cancel(a), " ", std.task.cancel(a), " ", std.task.alive(a));
		std.console.log(std.task.current());
		export fn same() { return a == a && a != b; }
	)");

	REQUIRE(host->Take() == Lines{ "b is task#2", "b goes on until it waits", "true false", "true false false", "undefined" });
	REQUIRE(script.TaskCount() == 0u);
	REQUIRE(script.Call("same").m_oValue == HostValue(true));

	host->clock = 5.0;
	RunTasks(script, 0);
	REQUIRE(host->Take().empty());
}

TEST_CASE("std.task | one task cancels another that's due in the same RunTasks") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		let second;
		std.task.spawn(() => { std.task.wait(); std.task.cancel(second); std.console.log("first"); });
		second = std.task.spawn(() => { std.task.wait(); std.console.log("second never"); });
	)");

	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "first" });
	REQUIRE(script.TaskCount() == 0u);
}

TEST_CASE("std.task | mistakes are errors") {
	auto host = std::make_unique<Host>();
	const auto fails = [&](std::string_view source, std::string_view message) {
		Script script(host->Options());
		REQUIRE(script.LoadSource(source).Ok());
		const auto result = script.Run();
		INFO(result.m_sError);
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_sError.find(message) != std::string::npos);
	};

	fails("std.task.wait(1);", "std.task.wait: only a task can wait");
	fails("const co = std.coroutine.create(() => { std.task.wait(1); }); std.coroutine.resume(co);", "only a task can wait");
	fails("std.task.spawn(() => { std.task.wait(-1); });", "the time must be a number of seconds that isn't negative");
	fails("std.task.spawn(() => { std.task.wait(1, 2); });", "arguments");
	fails("std.task.spawn(() => { std.array.for_each([1], (x) => { std.task.wait(); }); });", "can't yield inside a function that a native function called");
	fails("std.task.spawn(5);", "std.task.spawn: argument 1 must be a script function");
	fails("std.task.spawn((a) => { return a; });", "std.task.spawn: passed 0 arguments for the function, but it expects 1");
	fails("std.task.cancel(1);", "std.task.cancel: argument 1 must be \"task\"");
	fails("std.task.alive(\"task#1\");", "std.task.alive: argument 1 must be \"task\"");
}

TEST_CASE("std.task | errors: before the first wait in the spawner, later in RunTasks") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		try {
			std.task.spawn(() => { throw "early"; });
		} catch (e) {
			std.console.log("caught ", e);
		}
		std.task.spawn(() => { std.task.wait(); throw "late"; });
		std.task.spawn(() => { std.task.wait(); const x = "a" + 1; });
		std.task.spawn(() => { std.task.wait(); std.console.log("the others still run"); std.task.wait(); std.console.log("and again"); });
		export fn ok() { return 1; }
	)");

	REQUIRE(host->Take() == Lines{ "caught early" });
	REQUIRE(script.TaskCount() == 3u);

	const auto result = script.RunTasks();
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("uncaught exception: late") != std::string::npos);
	REQUIRE(host->Take() == Lines{ "the others still run" });
	REQUIRE(script.TaskCount() == 1u);

	// the script still works
	REQUIRE(script.Call("ok").m_oValue == HostValue(1));
	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "and again" });
}

TEST_CASE("std.task | a runtime error in a task shows where it happened") {
	auto host = std::make_unique<Host>();
	auto script = host->Start("std.task.spawn(() => {\n std.task.wait();\n const x = \"a\" + 1;\n});");

	const auto result = script.RunTasks();
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	INFO(result.m_sError);
	REQUIRE(result.m_sError.find("at [3, 15]") != std::string::npos);
	REQUIRE(script.TaskCount() == 0u);
}

TEST_CASE("std.task | each task gets its own instruction budget") {
	auto host = std::make_unique<Host>();
	auto options = host->Options();
	options.m_oLimits.m_uInstructions = 20'000;

	Script script(std::move(options));
	REQUIRE(script.LoadSource(R"(
		for (let i = 0; i < 5; i++) {
			std.task.spawn(() => {
				std.task.wait();
				let sum = 0;
				for (let j = 0; j < 1000; j++) { sum += j; }
			});
		}
		std.task.spawn(() => { std.task.wait(); while (true) { let z = 1; } });
	)").Ok());
	REQUIRE(script.Run().Ok());

	const auto result = script.RunTasks();
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("ran too long") != std::string::npos);
	REQUIRE(script.TaskCount() == 0u);
}

TEST_CASE("std.task | event handlers spawn tasks") {
	auto host = std::make_unique<Host>();
	auto script = host->Start(R"(
		events.on("door_opened", (door) => {
			std.task.spawn(() => {
				std.task.wait(10);
				std.console.log("closing ", door);
			});
		});
	)");

	REQUIRE(script.Emit("door_opened", { "front" }).Ok());
	host->clock = 4.0;
	REQUIRE(script.Emit("door_opened", { "back" }).Ok());
	REQUIRE(script.TaskCount() == 2u);

	host->clock = 10.0;
	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "closing front" });
	host->clock = 14.0;
	RunTasks(script, 1);
	REQUIRE(host->Take() == Lines{ "closing back" });
}

TEST_CASE("std.task | exit in a task, and loading again drops the tasks") {
	auto host = std::make_unique<Host>();
	{
		auto script = host->Start("std.task.spawn(() => { std.task.wait(); std.process.exit(3); });");
		// (std.process isn't allowed by default)
		const auto result = script.RunTasks();
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	}
	{
		auto options = host->Options();
		options.m_oEnvironment.m_bProcess = true;
		Script script(std::move(options));
		REQUIRE(script.LoadSource("std.task.spawn(() => { std.task.wait(); std.process.exit(3); });").Ok());
		REQUIRE(script.Run().Ok());
		const auto result = script.RunTasks();
		REQUIRE(result.m_eStatus == Result::Status::exited);
		REQUIRE(result.m_iExitCode == 3);
		REQUIRE(script.RunTasks().m_eStatus == Result::Status::usage_error);
	}
	{
		auto script = host->Start("std.task.spawn(() => { std.task.wait(); });");
		REQUIRE(script.TaskCount() == 1u);
		REQUIRE(script.LoadSource("return 1;").Ok());
		REQUIRE(script.TaskCount() == 0u);
	}
}

// (not tasks: compile errors carry where they are, for a host's error screen)
#include <filesystem>
#include <fstream>

TEST_CASE("embedding | compile errors have a location") {
	{
		Script script;
		const auto result = script.LoadSource("let a = 1;\nlet b = c + 1;\n", {}, "main.bloop");
		REQUIRE(result.m_eStatus == Result::Status::compile_error);
		REQUIRE(result.m_oLocation);
		INFO(result.m_sError);
		CHECK(result.m_oLocation->m_uLine == 2u);
		CHECK(result.m_oLocation->m_sSourceLine == "let b = c + 1;");
		CHECK(result.m_oLocation->m_sMessage.find('c') != std::string::npos);
		CHECK(result.m_oLocation->m_sMessage.find("at:") == std::string::npos);
	}
	{
		// in an imported file: that file and its own line
		const auto directory = std::filesystem::temp_directory_path() / "bloop_location_test";
		std::filesystem::create_directories(directory);
		// (CRLF on purpose, as an editor on windows saves it: the source line comes without the \r)
		std::ofstream(directory / "helper.bloop", std::ios::binary) << "export fn f() {\r\n\treturn 1;\r\n}\r\nexport fn g() {\r\n\treturn undefined_thing;\r\n}\r\n";
		std::ofstream(directory / "main.bloop", std::ios::binary) << "import f from \"helper.bloop\";\n\nreturn f();\n";

		Script script;
		const auto result = script.LoadFile(directory / "main.bloop");
		REQUIRE(result.m_eStatus == Result::Status::compile_error);
		INFO(result.m_sError);
		REQUIRE(result.m_oLocation);
		CHECK(result.m_oLocation->m_sFile.find("helper") != std::string::npos);
		CHECK(result.m_oLocation->m_uLine == 5u);
		CHECK(result.m_oLocation->m_sSourceLine == "\treturn undefined_thing;");
		CHECK(result.m_oLocation->m_uColumn > 1u);
	}
}

TEST_CASE("embedding | runtime errors have a location and a trace") {
	auto host = std::make_unique<Host>();
	{
		// an error in a function that a handler called
		auto script = host->Start("fn inner(x) {\n\treturn x + \"a\";\n}\nevents.on(\"hit\", (x) => {\n\tinner(x);\n});\n");
		const auto result = script.Emit("hit", { 1 });
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		INFO(result.m_sError);
		REQUIRE(result.m_oLocation);
		CHECK(result.m_oLocation->m_uLine == 2u);
		CHECK(result.m_oLocation->m_sSourceLine == "\treturn x + \"a\";");
		CHECK(!result.m_oLocation->m_sMessage.empty());
		REQUIRE(result.m_oTrace.size() == 2u);
		CHECK(result.m_oTrace[1].m_uLine == 5u);
		CHECK(result.m_oTrace[1].m_sMessage.empty());
	}
	{
		// an uncaught throw: where it was thrown, though the calls are gone by the time it's reported
		auto script = host->Start("fn deep() {\n\tthrow \"boom\";\n}\nfn mid() {\n\tdeep();\n}\nexport fn top() {\n\tmid();\n}\n");
		const auto result = script.Call("top");
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		INFO(result.m_sError);
		REQUIRE(result.m_oLocation);
		CHECK(result.m_oLocation->m_uLine == 2u);
		CHECK(result.m_oLocation->m_sMessage.find("boom") != std::string::npos);
		REQUIRE(result.m_oTrace.size() == 3u);
		CHECK(result.m_oTrace[1].m_uLine == 5u);
		CHECK(result.m_oTrace[2].m_uLine == 8u);
	}
	{
		// a caught throw leaves no trace behind for a later error
		auto script = host->Start("export fn f() {\n\ttry {\n\t\tthrow 1;\n\t} catch (e) {\n\t\tlet z = 1;\n\t}\n\treturn 1 + \"a\";\n}\n");
		const auto result = script.Call("f");
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_oTrace.size() == 1u);
		CHECK(result.m_oLocation->m_uLine == 7u);
	}
	{
		// in a task, after a wait: inside the task
		auto script = host->Start("std.task.spawn(() => {\n\tstd.task.wait();\n\tthrow \"late\";\n});\n");
		const auto result = script.RunTasks();
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		INFO(result.m_sError);
		REQUIRE(result.m_oLocation);
		CHECK(result.m_oLocation->m_uLine == 3u);
	}
	{
		// in a coroutine that a function resumed
		auto script = host->Start("const co = std.coroutine.create(() => {\n\tthrow \"in co\";\n});\nexport fn f() {\n\tstd.coroutine.resume(co);\n}\n");
		const auto result = script.Call("f");
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		INFO(result.m_sError);
		REQUIRE(result.m_oLocation);
		CHECK(result.m_oLocation->m_uLine == 2u);
		REQUIRE(result.m_oTrace.size() == 2u);
		CHECK(result.m_oTrace[1].m_uLine == 5u);
	}
}

#include <map>

TEST_CASE("embedding | the host reads the files") {
	// files that aren't on the disk (a game's archives)
	const auto root = std::filesystem::absolute("virtual_scripts_that_dont_exist");
	const std::map<std::filesystem::path, std::string> files = {
		{ root / "main.bloop", "import twice from \"lib/math.bloop\";\nexport fn run() { return twice(21); }\n" },
		{ root / "lib" / "math.bloop", "export fn twice(x) { return x * 2; }\n" },
		{ root / "broken.bloop", "let a = ;\n" },
	};
	std::vector<std::string> asked;

	ScriptOptions options;
	options.m_fnReadFile = [&](const std::filesystem::path& path) -> std::optional<std::string> {
		asked.push_back(path.lexically_relative(root).generic_string());
		const auto itr = files.find(path.lexically_normal());
		return itr == files.end() ? std::nullopt : std::optional<std::string>(itr->second);
	};

	{
		Script script(options);
		const auto loaded = script.LoadFile(root / "main.bloop");
		INFO(loaded.m_sError);
		REQUIRE(loaded.Ok());
		REQUIRE(script.Run().Ok());
		REQUIRE(script.Call("run").m_oValue == HostValue(42));
		REQUIRE(asked == std::vector<std::string>{ "main.bloop", "lib/math.bloop" });
	}
	{
		Script script(options);
		const auto loaded = script.LoadFile(root / "missing.bloop");
		REQUIRE(loaded.m_eStatus == Result::Status::compile_error);
		REQUIRE(loaded.m_sError.find("couldn't open") != std::string::npos);
	}
	{
		Script script(options);
		const auto loaded = script.LoadFile(root / "broken.bloop");
		REQUIRE(loaded.m_eStatus == Result::Status::compile_error);
		REQUIRE(loaded.m_oLocation);
		REQUIRE(loaded.m_oLocation->m_sSourceLine == "let a = ;");
	}
	{
		// imports stay inside the import root
		std::map<std::filesystem::path, std::string> escaping = { { root / "main.bloop", "import x from \"../outside.bloop\";\n" } };
		ScriptOptions sandboxed;
		sandboxed.m_fnReadFile = [&](const std::filesystem::path& path) -> std::optional<std::string> {
			const auto itr = escaping.find(path.lexically_normal());
			return itr == escaping.end() ? std::optional<std::string>("export const x = 1;") : std::optional<std::string>(itr->second);
		};
		Script script(sandboxed);
		const auto loaded = script.LoadFile(root / "main.bloop");
		REQUIRE(loaded.m_eStatus == Result::Status::compile_error);
		REQUIRE(loaded.m_sError.find("outside of the directory") != std::string::npos);
	}
}

// one global for everything of the host's: game.doors.open(...), game.events.on(...)
namespace {
	bloop::BloopInt NestedTwice(bloop::BloopInt x) { return x * 2; }
	bloop::BloopInt NestedAnswer() { return 42; }
}
TEST_CASE("embedding | nested host modules") {
	using namespace bloop::standard;
	NativeDef events(BLOOPTEXT("events"), NativeObject{});
	bloop::AddEventFunctions(events);

	NativeDef game(BLOOPTEXT("game"), NativeObject{ .m_oFields = {
		NativeField(BLOOPTEXT("answer"), Bind<&NestedAnswer>()),
		NativeField(BLOOPTEXT("math"), NativeDef(BLOOPTEXT("math"), NativeObject{ .m_oFields = {
			NativeField(BLOOPTEXT("twice"), Bind<&NestedTwice>()),
		}})),
		NativeField(BLOOPTEXT("events"), events),
	}});

	ScriptOptions options;
	options.m_oModules.push_back(game);
	Script script(options);
	REQUIRE(script.LoadSource(R"(
		let got = 0;
		game.events.on("hit", (x) => { got = game.math.twice(x) + game.answer(); });
		export fn result() { return got; }
	)").Ok());
	REQUIRE(script.Run().Ok());
	REQUIRE(script.Emit("hit", { 5 }).Ok());
	REQUIRE(script.Call("result").m_oValue == HostValue(52));

	// the parts aren't globals
	Script other(options);
	REQUIRE(other.LoadSource("events.on(\"x\", () => { return 1; });").m_eStatus == Result::Status::compile_error);
}
