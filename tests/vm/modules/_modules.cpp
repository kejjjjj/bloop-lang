#include "tests/vm/defs.hpp"

#include <string>
#include <initializer_list>
#include <stdexcept>
#include <filesystem>

#define PREFIX "modules"

namespace {
	// runs a file and returns what its return value prints as
	std::string Run(const bloop::BloopString& relative_path) {
		const auto ret = bloop::test::TEST_ExecuteFile(relative_path);
		INFO("output: " << bloop::test::TEST_LastOutput());
		REQUIRE(ret.has_value());
		return ret->ValueToString();
	}
	std::string Path(const char* dir, const char* name) {
		return JOIN_PATH(PREFIX, "") + JOIN_PATH(dir, name) + BLOOPTEXT(".bloop");
	}

	// compiling the file throws an error that contains every part of `parts`
	void ExpectCompileError(const bloop::BloopString& relative_path, std::initializer_list<std::string> parts) {
		std::string message;
		try {
			bloop::test::TEST_CompileFile(relative_path);
		} catch (std::runtime_error& ex) {
			message = ex.what();
		}
		INFO("error: " << message);
		REQUIRE(!message.empty());
		for (const auto& part : parts)
			CHECK(message.find(part) != std::string::npos);
	}
}

TEST_CASE("modules | import one name per statement") {
	REQUIRE(Run(Path("basic", "single")) == "[ 42, Hello! ]");
}
TEST_CASE("modules | import several names in one statement") {
	REQUIRE(Run(Path("basic", "list")) == "[ 42, Hello! ]");
}
TEST_CASE("modules | subdirectories, \"..\" and \"./\" are relative to the importing file") {
	REQUIRE(Run(Path("paths", "main")) == "[ helper uses config, config, leaf -> helper uses config ]");
}
TEST_CASE("modules | a module imported from several places runs once, dependencies run first") {
	REQUIRE(Run(Path("diamond", "main")) == "[ log,shared,left,right,main, 1, 1, 1 ]");
}
TEST_CASE("modules | every module has its own globals") {
	REQUIRE(Run(Path("scope", "main")) == "[ first1, second11, first2, main100 ]");
}
TEST_CASE("modules | globals that aren't exported can't be used by the importer") {
	ExpectCompileError(Path("scope", "private"), { "unknown identifier: calls" });
}
TEST_CASE("modules | exported functions are hoisted inside their module") {
	REQUIRE(Run(Path("scope", "hoisted_main")) == "[ 42, 21 ]");
}
TEST_CASE("modules | imports see the current value of exported variables") {
	REQUIRE(Run(Path("live", "main")) == "[ 0, 2, 3, 99 ]");
}
TEST_CASE("modules | imports can't be assigned to") {
	ExpectCompileError(Path("live", "assign_import"), { "const" });
	ExpectCompileError(Path("live", "assign_function"), { "const" });
}
TEST_CASE("modules | exceptions thrown in a module can be caught by the importer") {
	REQUIRE(Run(Path("live", "exceptions")) == "failed with 7");
}
TEST_CASE("modules | runtime errors name the file and its own line") {
	const auto ret = bloop::test::TEST_ExecuteFile(Path("live", "runtime_error"));
	const auto& output = bloop::test::TEST_LastOutput();
	INFO(output);
	REQUIRE(ret.has_value());
	REQUIRE(ret->type == Value::Type::t_undefined);
	REQUIRE(output.find("out of bounds index [5]") != std::string::npos);
	REQUIRE(output.find("at [exceptions_module.bloop:6, ") != std::string::npos);
	REQUIRE(output.find("return arr[5];") != std::string::npos);
	REQUIRE(output.find("at [runtime_error.bloop:3, ") != std::string::npos);
}
TEST_CASE("modules | circular imports are an error that shows the chain") {
	ExpectCompileError(Path("circular", "main"), { "circular import: a.bloop -> b.bloop -> a.bloop", "b.bloop:1:" });
	ExpectCompileError(Path("circular", "back_to_main"), { "circular import: back_to_main.bloop -> c.bloop -> back_to_main.bloop" });
	ExpectCompileError(Path("circular", "self"), { "circular import: self.bloop -> self.bloop" });
}
TEST_CASE("modules | importing a name that isn't exported") {
	ExpectCompileError(Path("errors", "missing_export"), { "\"../basic/my_module.bloop\" doesn't export \"missing\"", "missing_export.bloop:1:" });
}
TEST_CASE("modules | importing a file that doesn't exist") {
	ExpectCompileError(Path("errors", "missing_file"), { "couldn't open the module \"does_not_exist.bloop\"" });
}
TEST_CASE("modules | errors in an imported file name that file and its own line") {
	ExpectCompileError(Path("errors", "bad_syntax"), { "bad_syntax_module.bloop:3:" });
	ExpectCompileError(Path("errors", "unknown"), { "unknown identifier: not_declared_anywhere", "unknown_in_module.bloop:3:" });
}
TEST_CASE("modules | an imported module can't return from its global scope") {
	ExpectCompileError(Path("errors", "return_in_module"), { "can't return from its global scope", "return_module.bloop:2:" });
}
TEST_CASE("modules | imported names can't be declared again") {
	ExpectCompileError(Path("errors", "import_twice"), { "identifier \"value\" already declared" });
	ExpectCompileError(Path("errors", "import_conflict"), { "identifier \"value\" already declared" });
	ExpectCompileError(Path("errors", "import_conflicts_function"), { "identifier \"greet\" already declared" });
}

// inline scripts: their imports are relative to tests/vm
TEST_CASE("modules | export and import are only allowed in the right places") {
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("fn f() { export const a = 1; }"), Catch::Matchers::ContainsSubstring("only declarations in the global scope can be exported"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("if (true) { export let a = 1; }"), Catch::Matchers::ContainsSubstring("only declarations in the global scope can be exported"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("export 5;"), Catch::Matchers::ContainsSubstring("can be exported"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("export"), Catch::Matchers::ContainsSubstring("expected a declaration"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("const a = 1;\nimport value from \"modules/basic/my_module.bloop\";"),
		Catch::Matchers::ContainsSubstring("imports must come before everything else"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("fn f() { import value from \"modules/basic/my_module.bloop\"; }"),
		Catch::Matchers::ContainsSubstring("imports are only allowed at the top of the file"));
}
TEST_CASE("modules | import syntax errors") {
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import from \"x.bloop\";"), Catch::Matchers::ContainsSubstring("expected \",\" or \"from\""));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import a b from \"x.bloop\";"), Catch::Matchers::ContainsSubstring("expected \"from\""));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import a, ;"), Catch::Matchers::ContainsSubstring("expected a name to import"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import a from x;"), Catch::Matchers::ContainsSubstring("the path of the module"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import a from \"\";"), Catch::Matchers::ContainsSubstring("the path of the module is empty"));
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileBuffer("import a from \"x.bloop\""), Catch::Matchers::ContainsSubstring("expected \";\""));
}
TEST_CASE("modules | a script that isn't in a file can import too") {
	const auto ret = bloop::test::TEST_ExecuteBuffer("import value, greet from \"modules/basic/my_module.bloop\";\nreturn greet() + std.string.from(value);");
	REQUIRE(ret.has_value());
	REQUIRE(ret->ValueToString() == "Hello!42");
}
TEST_CASE("modules | \"from\" is still an ordinary name") {
	const auto ret = bloop::test::TEST_ExecuteBuffer("const from = 5;\nreturn std.string.from(from);");
	REQUIRE(ret.has_value());
	REQUIRE(ret->ValueToString() == "5");
}
TEST_CASE("modules | a host can keep imports inside a directory") {
	const auto modules = std::filesystem::path(SOURCE_DIR) / "vm" / "modules";

	// everything paths/main.bloop imports is inside paths/
	bloop::module::CompileOptions options;
	options.m_oImportRoot = modules / "paths";
	REQUIRE_NOTHROW(bloop::test::TEST_CompileFile(Path("paths", "main"), options));

	// helper.bloop imports "../config.bloop", which is outside of paths/utils
	options.m_oImportRoot = modules / "paths" / "utils";
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileFile(Path("paths", "main"), options),
		Catch::Matchers::ContainsSubstring("can't import \"../config.bloop\": it's outside of the directory that imports are allowed from"));

	options.m_oImportRoot = modules / "errors";
	REQUIRE_THROWS_WITH(bloop::test::TEST_CompileFile(Path("errors", "missing_export"), options),
		Catch::Matchers::ContainsSubstring("can't import \"../basic/my_module.bloop\""));
}
