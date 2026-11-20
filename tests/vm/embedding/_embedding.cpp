#include "bloop.hpp"
#include "tests/vm/defs.hpp"

#include <filesystem>
#include <string>
#include <vector>

using bloop::HostValue;
using bloop::Result;
using bloop::Script;
using bloop::ScriptOptions;

namespace {
#ifdef BLOOP_GC_STRESS
	constexpr auto SCALE = 10; // a collection after every allocation: smaller loops
#else
	constexpr auto SCALE = 1;
#endif

	// loads and runs, failing the test on errors
	Script Start(std::string_view source, ScriptOptions options = {}) {
		Script script(std::move(options));
		const auto loaded = script.LoadSource(source);
		INFO(loaded.m_sError);
		REQUIRE(loaded.Ok());
		const auto ran = script.Run();
		INFO(ran.m_sError);
		REQUIRE(ran.Ok());
		return script;
	}

	std::filesystem::path ScriptDirectory() {
		return std::filesystem::path(SOURCE_DIR) / "vm";
	}

	// ---- a host module ----
	struct Game {
		std::vector<std::string> spawned;
	};

	bloop::BloopInt Spawn(bloop::vm::VM& vm, bloop::standard::StringArg name, bloop::BloopInt count) {
		auto* game = vm.GetUserData<Game>();
		for (bloop::BloopInt i = 0; i < count; i++)
			game->spawned.emplace_back(name.View());
		return static_cast<bloop::BloopInt>(game->spawned.size());
	}
	// calls a script callback for every spawned thing
	void ForEachSpawned(bloop::vm::VM& vm, bloop::standard::CallableArg callback) {
		auto* game = vm.GetUserData<Game>();
		for (const auto& name : game->spawned) {
			bloop::vm::RootScope scope(vm.GetGC());
			const auto arg = scope.Keep(bloop::FromHostValue(vm, HostValue(name)));
			const bloop::vm::Value args[] = { arg.Get() };
			[[maybe_unused]] const auto result = vm.Call(callback.Get(), args);
		}
	}

	bloop::standard::NativeDef GameModule() {
		using namespace bloop::standard;
		return NativeDef(BLOOPTEXT("game"), NativeObject{ .m_oFields = {
			NativeField(BLOOPTEXT("spawn"), Bind<&Spawn>()),
			NativeField(BLOOPTEXT("for_each_spawned"), Bind<&ForEachSpawned>()),
			NativeField(BLOOPTEXT("version"), MakeConstant(bloop::BloopInt{ 3 })),
		}});
	}
}

TEST_CASE("embedding | load, run and call exported functions") {
	auto script = Start(R"(
		export fn add(a, b) { return a + b; }
		export const name = "test mod";
		fn hidden() { return 1; }
		return "top level done";
	)");

	REQUIRE(script.HasRun());
	REQUIRE(script.HasFunction("add"));
	REQUIRE_FALSE(script.HasFunction("hidden")); // not exported
	REQUIRE_FALSE(script.HasFunction("name"));   // not a function
	REQUIRE(script.Exports() == std::vector<std::string>{ "add", "name" });

	const auto sum = script.Call("add", { 2, 3 });
	REQUIRE(sum.Ok());
	REQUIRE(sum.m_oValue == HostValue(5));

	REQUIRE(script.Call("add", { "a", "b" }).m_oValue == HostValue("ab"));
	REQUIRE(script.Call("add", { 0.5, 0.25 }).m_oValue == HostValue(0.75));
	REQUIRE(script.Get("name").m_oValue == HostValue("test mod"));
}

TEST_CASE("embedding | the result of Run is the top level return value") {
	Script script;
	REQUIRE(script.LoadSource("return [1, \"two\", { three: 3 }];").Ok());
	const auto result = script.Run();
	REQUIRE(result.Ok());
	REQUIRE(result.m_oValue == HostValue(HostValue::Array{ 1, "two", HostValue::Object{ { "three", 3 } } }));
	REQUIRE(result.m_oValue.ToString() == "[1, \"two\", {\"three\": 3}]");
}

TEST_CASE("embedding | values in and out") {
	auto script = Start("export fn echo(x) { return x; }\nexport fn make_fn() { return () => 1; }");

	const HostValue nested = HostValue::Object{
		{ "z", HostValue::Array{ 1, 2.5, true, HostValue{}, "text" } },
		{ "a", HostValue::Object{ { "deep", HostValue::Array{} } } },
		{ "big", bloop::BloopInt{ 9007199254740993 } },
	};
	const auto echoed = script.Call("echo", { nested });
	REQUIRE(echoed.Ok());
	REQUIRE(echoed.m_oValue == nested); // object keys keep their order

	REQUIRE(echoed.m_oValue.Find("z")->AsArray()[1].AsDouble() == 2.5);
	REQUIRE(echoed.m_oValue.Find("missing") == nullptr);

	// functions can't be copied out
	const auto fn = script.Call("make_fn");
	REQUIRE(fn.Ok());
	REQUIRE(fn.m_oValue.IsOpaque());
}

TEST_CASE("embedding | state stays between calls") {
	auto script = Start(R"(
		let ticks = 0;
		let total = 0.0;
		export fn on_update(dt) {
			ticks++;
			total += dt;
			return [ticks, total];
		}
	)");

	for (auto i = 1; i <= 100; i++) {
		const auto result = script.Call("on_update", { 0.5 });
		REQUIRE(result.Ok());
		REQUIRE(result.m_oValue.AsArray()[0] == HostValue(i));
	}
	REQUIRE(script.Call("on_update", { 0.5 }).m_oValue.AsArray()[1] == HostValue(50.5));
}

TEST_CASE("embedding | errors are results, and the script keeps working") {
	SECTION("compile errors") {
		Script script;
		const auto result = script.LoadSource("let x = ;");
		REQUIRE(result.m_eStatus == Result::Status::compile_error);
		REQUIRE_FALSE(result.Ok());
		REQUIRE(result.m_sError.find("1:9") != std::string::npos);
		REQUIRE_FALSE(script.IsLoaded());
		REQUIRE(script.Run().m_eStatus == Result::Status::usage_error);
	}
	SECTION("runtime errors in the top level code") {
		Script script;
		REQUIRE(script.LoadSource("const a = [1];\nreturn a[5];").Ok());
		const auto result = script.Run();
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_sError.find("out of bounds index [5]") != std::string::npos);
		REQUIRE(result.m_sError.find("return a[5];") != std::string::npos); // the stack trace
	}
	SECTION("runtime errors in calls") {
		auto script = Start(R"(
			export fn fail(i) { const a = [1]; return a[i]; }
			export fn boom() { throw "boom"; }
			export fn caught() { try { throw "x"; } catch (e) { return "caught " + e; } }
			export fn ok() { return "ok"; }
		)");

		auto result = script.Call("fail", { 9 });
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_sError.find("out of bounds index [9]") != std::string::npos);

		REQUIRE(script.Call("ok").m_oValue == HostValue("ok")); // still usable

		result = script.Call("boom");
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_sError.find("uncaught exception: boom") != std::string::npos);

		REQUIRE(script.Call("caught").m_oValue == HostValue("caught x"));
		REQUIRE(script.Call("fail", { 0 }).m_oValue == HostValue(1));

		result = script.Call("ok", { 1 }); // strict argument counts
		REQUIRE(result.m_eStatus == Result::Status::runtime_error);
		REQUIRE(result.m_sError.find("passed 1 arguments, but expected 0") != std::string::npos);
	}
	SECTION("using the script wrong") {
		Script script;
		REQUIRE(script.Call("x").m_eStatus == Result::Status::usage_error); // nothing loaded

		REQUIRE(script.LoadSource("export fn f() { return 1; }\nexport const n = 5;").Ok());
		REQUIRE(script.Call("f").m_eStatus == Result::Status::usage_error); // before Run
		REQUIRE(script.Get("n").m_eStatus == Result::Status::usage_error);
		REQUIRE(script.Run().Ok());
		REQUIRE(script.Run().m_eStatus == Result::Status::usage_error); // twice

		auto result = script.Call("missing");
		REQUIRE(result.m_eStatus == Result::Status::usage_error);
		REQUIRE(result.m_sError.find("doesn't export \"missing\"") != std::string::npos);

		result = script.Call("n");
		REQUIRE(result.m_eStatus == Result::Status::usage_error);
		REQUIRE(result.m_sError.find("isn't a function") != std::string::npos);
	}
}

TEST_CASE("embedding | std.process.exit") {
	ScriptOptions options;
	options.m_oEnvironment.m_bProcess = true;
	auto script = Start("export fn quit(code) { std.process.exit(code); }\nexport fn f() { return 1; }", options);

	const auto result = script.Call("quit", { 7 });
	REQUIRE(result.m_eStatus == Result::Status::exited);
	REQUIRE(result.Ok());
	REQUIRE(result.m_iExitCode == 7);
	REQUIRE(script.HasExited());

	const auto after = script.Call("f");
	REQUIRE(after.m_eStatus == Result::Status::usage_error);
	REQUIRE(after.m_sError.find("exited") != std::string::npos);
}

TEST_CASE("embedding | host modules") {
	Game game;
	ScriptOptions options;
	options.m_oModules.push_back(GameModule());
	options.m_pUserData = &game;

	auto script = Start(R"(
		const total = game.spawn("crate", 2);
		export fn spawn_more(name) { return game.spawn(name, 1); }
		export fn names() {
			const result = [];
			game.for_each_spawned((name) => { std.array.push(result, name); });
			return result;
		}
		export fn version() { return game.version; }
	)", options);

	REQUIRE(game.spawned == std::vector<std::string>{ "crate", "crate" });
	REQUIRE(script.Call("spawn_more", { "barrel" }).m_oValue == HostValue(3));
	REQUIRE(script.Call("names").m_oValue == HostValue(HostValue::Array{ "crate", "crate", "barrel" }));
	REQUIRE(script.Call("version").m_oValue == HostValue(3));

	// wrong arguments are runtime errors, like with std
	const auto result = script.Call("spawn_more", { 5 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("argument 1 must be \"string\"") != std::string::npos);
}

TEST_CASE("embedding | host modules can be used by imported modules too") {
	Game game;
	ScriptOptions options;
	options.m_oModules.push_back(GameModule());
	options.m_pUserData = &game;

	Script script(options);
	REQUIRE(script.LoadSource("import name from \"modules/paths/config.bloop\";\nreturn game.spawn(name, 1);", ScriptDirectory()).Ok());
	REQUIRE(script.Run().m_oValue == HostValue(1));
	REQUIRE(game.spawned == std::vector<std::string>{ "config" });
}

TEST_CASE("embedding | a host module's name can't be taken twice") {
	ScriptOptions options;
	options.m_oModules.push_back(GameModule());
	options.m_oModules.push_back(GameModule());
	Script script(options);
	const auto result = script.LoadSource("return 1;");
	REQUIRE(result.m_eStatus == Result::Status::compile_error);
	REQUIRE(result.m_sError.find("\"game\" is defined more than once") != std::string::npos);

	// and the script can't declare it either
	ScriptOptions single;
	single.m_oModules.push_back(GameModule());
	Script script2(single);
	REQUIRE(script2.LoadSource("let game = 1;").m_eStatus == Result::Status::compile_error);
}

TEST_CASE("embedding | instruction budget") {
	ScriptOptions options;
	options.m_oLimits.m_uInstructions = 100'000;
	auto script = Start(R"(
		export fn forever() { let x = 0; while (true) { x++; } }
		export fn recurse(n) { return recurse(n + 1) + 1; }
		export fn some_work() { let x = 0; for (let i = 0; i < 1000; i++) { x += i; } return x; }
	)", options);

	auto result = script.Call("forever");
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("ran too long (more than 100000 instructions)") != std::string::npos);

	// every call gets a new budget
	for (auto i = 0; i < 20; i++)
		REQUIRE(script.Call("some_work").m_oValue == HostValue(499500));

	result = script.Call("recurse", { 0 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error); // (too deep, before the budget runs out)
	REQUIRE(script.Call("some_work").Ok());

	// the top level code too
	Script looping(options);
	REQUIRE(looping.LoadSource("let i = 0; while (true) { i++; }").Ok());
	REQUIRE(looping.Run().m_eStatus == Result::Status::runtime_error);
}

TEST_CASE("embedding | heap limit") {
#ifdef BLOOP_GC_STRESS
	SKIP("fills megabytes of memory: far too slow with a collection after every allocation");
#endif
	ScriptOptions options;
	options.m_oLimits.m_uHeapBytes = 8u * bloop::MB;
	auto script = Start(R"(
		let kept = [];
		export fn hog() {
			const a = [];
			while (true)
				std.array.push(a, [1, 2, 3, 4, 5, 6, 7, 8]);
		}
		export fn keep(n) {
			for (let i = 0; i < n; i++)
				std.array.push(kept, "item " + std.string.from(i));
			return std.length(kept);
		}
		export fn garbage() {
			// lots of short lived garbage is fine
			let n = 0;
			for (let i = 0; i < 200000; i++) { const tmp = [i, i]; n += tmp[1]; }
			return n;
		}
		export fn huge() { return std.string.clone("x", 50000000); }
	)", options);

	auto result = script.Call("hog");
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("out of memory") != std::string::npos);

	// the hog's array was garbage, so there's room again
	REQUIRE(script.Call("garbage").m_oValue == HostValue(bloop::BloopInt{ 19999900000 }));
	REQUIRE(script.Call("keep", { 1000 }).m_oValue == HostValue(1000));

	// a single allocation over the limit
	result = script.Call("huge");
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("out of memory") != std::string::npos);

	REQUIRE(script.Call("keep", { 1 }).m_oValue == HostValue(1001)); // what was kept is still there
}

TEST_CASE("embedding | the environment") {
	SECTION("std.fs and std.process don't exist by default") {
		auto script = Start("export fn has(name) { return std.object.has(std, name); }");
		REQUIRE(script.Call("has", { "fs" }).m_oValue == HostValue(false));
		REQUIRE(script.Call("has", { "process" }).m_oValue == HostValue(false));
		REQUIRE(script.Call("has", { "json" }).m_oValue == HostValue(true));
	}
	SECTION("console output goes where the host wants") {
		std::vector<std::string> lines;
		ScriptOptions options;
		options.m_oEnvironment.m_fnPrint = [&lines](bloop::BloopStringView line) { lines.emplace_back(line); };
		auto script = Start("std.console.log(\"hello \", 42);\nexport fn say(x) { std.console.log(x); }", options);
		REQUIRE(script.Call("say", { "again" }).Ok());
		REQUIRE(lines == std::vector<std::string>{ "hello 42", "again" });
	}
}

TEST_CASE("embedding | imports stay inside the main file's directory by default") {
	const auto utils = ScriptDirectory() / "modules" / "paths" / "utils";

	// helper.bloop imports "../config.bloop"
	Script script;
	auto result = script.LoadSource("import describe from \"helper.bloop\";\nreturn describe();", utils);
	REQUIRE(result.m_eStatus == Result::Status::compile_error);
	REQUIRE(result.m_sError.find("outside of the directory that imports are allowed from") != std::string::npos);

	// a wider root
	ScriptOptions wider;
	wider.m_oImportRoot = utils.parent_path();
	Script script2(wider);
	REQUIRE(script2.LoadSource("import describe from \"helper.bloop\";\nreturn describe();", utils).Ok());
	REQUIRE(script2.Run().m_oValue == HostValue("helper uses config"));

	// no limit
	ScriptOptions unrestricted;
	unrestricted.m_bUnrestrictedImports = true;
	Script script3(unrestricted);
	REQUIRE(script3.LoadSource("import describe from \"helper.bloop\";\nreturn describe();", utils).Ok());
	REQUIRE(script3.Run().Ok());

	// files: everything paths/main.bloop imports is inside paths/
	Script script4;
	REQUIRE(script4.LoadFile(ScriptDirectory() / "modules" / "paths" / "main.bloop").Ok());
	REQUIRE(script4.Run().Ok());
}

TEST_CASE("embedding | scripts are independent and movable") {
	auto a = Start("let n = 0;\nexport fn next() { n++; return n; }");
	auto b = Start("let n = 100;\nexport fn next() { n++; return n; }");

	REQUIRE(a.Call("next").m_oValue == HostValue(1));
	REQUIRE(b.Call("next").m_oValue == HostValue(101));

	Script moved = std::move(a);
	REQUIRE(moved.Call("next").m_oValue == HostValue(2));

	// loading again starts over
	REQUIRE(moved.LoadSource("export fn next() { return -1; }").Ok());
	REQUIRE_FALSE(moved.HasRun());
	REQUIRE(moved.Run().Ok());
	REQUIRE(moved.Call("next").m_oValue == HostValue(-1));
}

TEST_CASE("embedding | many calls with objects (gc)") {
	auto script = Start(R"(
		let all = [];
		export fn add(item) { std.array.push(all, item); return std.length(all); }
		export fn names() { return std.array.transform(all, (x) => x.name); }
	)");

	constexpr auto count = 3000 / SCALE;
	for (auto i = 0; i < count; i++) {
		const HostValue item = HostValue::Object{ { "name", "item" + std::to_string(i) }, { "data", HostValue::Array{ i, i * 2 } } };
		REQUIRE(script.Call("add", { item }).m_oValue == HostValue(i + 1));
	}
	const auto names = script.Call("names");
	REQUIRE(names.Ok());
	REQUIRE(names.m_oValue.AsArray().size() == static_cast<std::size_t>(count));
	REQUIRE(names.m_oValue.AsArray()[count - 1] == HostValue("item" + std::to_string(count - 1)));
}

namespace {
	// callbacks that a native function keeps for later
	std::vector<bloop::Function> g_oKept;

	void Later(bloop::vm::VM& vm, bloop::standard::CallableArg callback) {
		g_oKept.push_back(bloop::Function::Keep(vm, callback.Get()));
	}

	bloop::standard::NativeDef EventfulGameModule() {
		using namespace bloop::standard;
		auto module = NativeDef(BLOOPTEXT("game"), NativeObject{ .m_oFields = {
			NativeField(BLOOPTEXT("later"), Bind<&Later>()),
		}});
		bloop::AddEventFunctions(module);
		return module;
	}

	ScriptOptions EventOptions() {
		ScriptOptions options;
		options.m_oModules.push_back(EventfulGameModule());
		return options;
	}
}

TEST_CASE("embedding | events") {
	auto script = Start(R"(
		export let log = [];
		game.on("hit", (who, damage) => { std.array.push(log, who + " took " + std.string.from(damage)); });
		game.on("hit", (who, damage) => { std.array.push(log, "second handler"); });
		export fn count() { return std.length(log); }
	)", EventOptions());

	REQUIRE(script.HasHandlers("hit"));
	REQUIRE_FALSE(script.HasHandlers("nothing"));

	auto result = script.Emit("hit", { "bob", 5 });
	REQUIRE(result.Ok());
	REQUIRE(result.m_oValue == HostValue(2)); // handlers called

	REQUIRE(script.Emit("nothing").m_oValue == HostValue(0));
	REQUIRE(script.Get("log").m_oValue == HostValue(HostValue::Array{ "bob took 5", "second handler" }));
}

TEST_CASE("embedding | events: on and off") {
	auto script = Start(R"(
		export let calls = [];
		fn a() { std.array.push(calls, "a"); }
		fn b() { std.array.push(calls, "b"); }
		game.on("e", a);
		game.on("e", a); // the same handler twice: once
		game.on("e", b);

		export fn remove_a() { return game.off("e", a); }
		export fn remove_missing() { return game.off("e", () => 1); }
		export fn remove_unknown_event() { return game.off("unknown", a); }
	)", EventOptions());

	REQUIRE(script.Emit("e").m_oValue == HostValue(2));
	REQUIRE(script.Call("remove_a").m_oValue == HostValue(true));
	REQUIRE(script.Call("remove_a").m_oValue == HostValue(false));
	REQUIRE(script.Call("remove_missing").m_oValue == HostValue(false));
	REQUIRE(script.Call("remove_unknown_event").m_oValue == HostValue(false));
	REQUIRE(script.Emit("e").m_oValue == HostValue(1));
	REQUIRE(script.Get("calls").m_oValue == HostValue(HostValue::Array{ "a", "b", "b" }));
}

TEST_CASE("embedding | events: handlers that change the handlers") {
	auto script = Start(R"(
		export let calls = [];
		fn second() { std.array.push(calls, "second"); }
		fn added() { std.array.push(calls, "added"); }
		fn first() {
			std.array.push(calls, "first");
			game.off("e", second); // removed before its turn: not called
			game.on("e", added);   // added during the event: called from the next event on
			game.off("e", first);  // removing itself is fine
		}
		game.on("e", first);
		game.on("e", second);
	)", EventOptions());

	REQUIRE(script.Emit("e").m_oValue == HostValue(1));
	REQUIRE(script.Emit("e").m_oValue == HostValue(1));
	REQUIRE(script.Get("calls").m_oValue == HostValue(HostValue::Array{ "first", "added" }));
}

TEST_CASE("embedding | events: a failing handler doesn't stop the others") {
	auto script = Start(R"(
		export let calls = 0;
		game.on("e", (x) => { calls++; });
		game.on("e", (x) => { const a = []; return a[x]; });
		game.on("e", (x) => { calls++; });
		game.on("wrong", (a, b) => { calls++; }); // emitted with one argument
	)", EventOptions());

	auto result = script.Emit("e", { 3 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("out of bounds index [3]") != std::string::npos);
	REQUIRE(script.Get("calls").m_oValue == HostValue(2));

	result = script.Emit("wrong", { 1 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("passed 1 arguments, but expected 2") != std::string::npos);
}

TEST_CASE("embedding | events: every handler gets the same arguments") {
	auto script = Start(R"(
		export let seen = [];
		game.on("e", (list) => { std.array.push(list, "first was here"); });
		game.on("e", (list) => { std.array.push(seen, std.length(list)); });
	)", EventOptions());

	REQUIRE(script.Emit("e", { HostValue::Array{ 1 } }).Ok());
	REQUIRE(script.Get("seen").m_oValue == HostValue(HostValue::Array{ 2 }));
}

TEST_CASE("embedding | events: handlers survive garbage collections") {
	auto script = Start(R"(
		export fn subscribe(n) {
			for (let i = 0; i < n; i++) {
				const state = { id: i, hits: 0 }; // only the handler keeps this alive
				game.on("tick", (amount) => { state.hits += amount; });
			}
		}
		export fn churn(n) {
			let garbage = [];
			for (let i = 0; i < n; i++) { garbage = [i, "x" + std.string.from(i)]; }
			std.gc_major();
		}
		let total = 0;
		game.on("tick", (amount) => { total += amount; });
		export fn get_total() { return total; }
	)", EventOptions());

	REQUIRE(script.Call("subscribe", { 50 }).Ok());
	for (auto i = 0; i < 5; i++) {
		REQUIRE(script.Call("churn", { 20000 / SCALE }).Ok());
		REQUIRE(script.Emit("tick", { 2 }).m_oValue == HostValue(51));
	}
	REQUIRE(script.Call("get_total").m_oValue == HostValue(10));
}

TEST_CASE("embedding | events: exit and errors") {
	ScriptOptions options = EventOptions();
	options.m_oEnvironment.m_bProcess = true;
	auto script = Start("game.on(\"quit\", () => { std.process.exit(4); });\ngame.on(\"quit\", () => { std.console.log(\"not called\"); });", options);

	const auto result = script.Emit("quit");
	REQUIRE(result.m_eStatus == Result::Status::exited);
	REQUIRE(result.m_iExitCode == 4);
	REQUIRE(script.Emit("quit").m_eStatus == Result::Status::usage_error);

	// before Run
	Script notRun(EventOptions());
	REQUIRE(notRun.LoadSource("game.on(\"e\", () => 1);").Ok());
	REQUIRE(notRun.Emit("e").m_eStatus == Result::Status::usage_error);

	// a handler has to be callable
	Script bad(EventOptions());
	REQUIRE(bad.LoadSource("game.on(\"e\", 5);").Ok());
	REQUIRE(bad.Run().m_eStatus == Result::Status::runtime_error);
}

TEST_CASE("embedding | events module") {
	ScriptOptions options;
	options.m_sEventsModule = "events";
	auto script = Start("export let n = 0;\nevents.on(\"ping\", () => { n++; });", options);
	REQUIRE(script.Emit("ping").Ok());
	REQUIRE(script.Get("n").m_oValue == HostValue(1));
}

TEST_CASE("embedding | functions that the host keeps") {
	g_oKept.clear();
	auto script = Start(R"(
		let counter = 0;
		game.later((step) => { counter += step; return counter; });
		export fn twice(x) { return x * 2; }
	)", EventOptions());

	REQUIRE(g_oKept.size() == 1u);
	REQUIRE(script.Call(g_oKept[0], { 5 }).m_oValue == HostValue(5));
	REQUIRE(script.Call(g_oKept[0], { 5 }).m_oValue == HostValue(10));

	// exported functions
	const auto twice = script.GetFunction("twice");
	REQUIRE(twice.IsValid());
	REQUIRE(script.Call(twice, { 21 }).m_oValue == HostValue(42));
	REQUIRE_FALSE(script.GetFunction("missing").IsValid());

	// a function of another script
	auto other = Start("export fn f() { return 1; }");
	const auto f = other.GetFunction("f");
	REQUIRE(script.Call(f, {}).m_eStatus == Result::Status::usage_error);
	REQUIRE(other.Call(f, {}).m_oValue == HostValue(1));

	// loading again invalidates them
	REQUIRE(script.LoadSource("return 1;").Ok());
	REQUIRE_FALSE(g_oKept[0].IsValid());
	REQUIRE_FALSE(twice.IsValid());
	REQUIRE(script.Run().Ok());
	REQUIRE(script.Call(twice, {}).m_eStatus == Result::Status::usage_error);

	g_oKept.clear(); // (destroying them after their VM is fine)
}

// every script call recurses in C++, so infinite recursion has to be an error before the native stack runs out
// (it crashed with a stack overflow in msvc debug builds, where the stack is 1MB and the frames are big)
TEST_CASE("embedding | deep recursion is an error, not a crash") {
	auto script = Start(R"(
		export fn recurse(n) { return recurse(n + 1) + 1; }
		export fn depth(n) { if (n === 0) { return 0; } return depth(n - 1) + 1; }
		export fn sort_recurse(n) { return std.array.sort([2, 1], (a, b) => sort_recurse(n + 1)); } // through a native
	)");

	// script functions calling each other don't use the C++ stack, so this always stops at the frame limit
	auto result = script.Call("recurse", { 0 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);
	REQUIRE(result.m_sError.find("exceeded 1024 call frames") != std::string::npos);
	REQUIRE(result.m_sError.find("more calls)") != std::string::npos); // the stack trace is shortened

	result = script.Call("sort_recurse", { 0 });
	REQUIRE(result.m_eStatus == Result::Status::runtime_error);

	// so does anything below it, in every build
	REQUIRE(script.Call("depth", { 1000 }).m_oValue == HostValue(1000));
}

TEST_CASE("deep recursion in a plain script") {
	const auto ret = bloop::test::TEST_ExecuteBuffer("fn f(n) { return f(n + 1); }\nreturn f(0);");
	REQUIRE(ret.has_value());
	REQUIRE(ret->type == Value::Type::t_undefined);
	REQUIRE(bloop::test::TEST_GetLastVM()->Failed());
}
