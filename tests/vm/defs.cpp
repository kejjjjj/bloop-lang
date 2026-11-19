#include "tests/vm/defs.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "ast/ast.hpp"
#include "bytecode/build.hpp"
#include "bytecode/function/bc_function.hpp"
#include "vm/vm.hpp"
#include "utils/fmt.hpp"
#include "metadata/metadata.hpp"
#include "module/module.hpp"

#include <memory>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <functional>

using namespace bloop::vm;

bloop::BloopString LoadScript(bloop::BloopString path) {
    std::ifstream file(path);
    if (!file) 
		FAIL(bloop::fmt::format(BLOOPTEXT("failed to open script: \"{}\""), path));
    return bloop::BloopString((std::istreambuf_iterator<bloop::BloopChar>(file)), std::istreambuf_iterator<bloop::BloopChar>());
}

static std::filesystem::path ScriptDirectory() {
	return std::filesystem::path(SOURCE_DIR) / "vm";
}

static std::optional<Value> Execute(const std::function<std::unique_ptr<bloop::ast::Program>(bloop::metadata::Metadata&)>& compile,
	const bloop::standard::Environment& environment);

std::optional<Value> bloop::test::TEST_ExecuteFile(const bloop::BloopString& relative_path, const bloop::standard::Environment& environment) {

	const auto rel_path = std::filesystem::path("vm") / relative_path;
	std::cout << rel_path << '\n';
	const auto this_path = std::filesystem::path(SOURCE_DIR) / rel_path;

	if (!std::filesystem::exists(this_path))
		FAIL(bloop::fmt::format(BLOOPTEXT("failed to open script: \"{}\""), this_path.string()));

	// compiled as a file, so that its imports are relative to it
	return Execute([&](bloop::metadata::Metadata& metadata) { return bloop::module::CompileFile(this_path, metadata); }, environment);
}

void bloop::test::TEST_CompileFile(const bloop::BloopString& relative_path, const bloop::module::CompileOptions& options) {
	const auto path = ScriptDirectory() / relative_path;

	if (!std::filesystem::exists(path))
		FAIL(bloop::fmt::format(BLOOPTEXT("failed to open script: \"{}\""), path.string()));

	bloop::metadata::Metadata metadata;
	[[maybe_unused]] const auto code = bloop::module::CompileFile(path, metadata, options);
}

void bloop::test::TEST_CompileBuffer(bloop::BloopStringView buffer) {
	bloop::metadata::Metadata metadata;
	[[maybe_unused]] const auto code = bloop::module::CompileSource(buffer, metadata, ScriptDirectory());
}

//silly, but it doesn't destroy the vm
static std::unique_ptr<bloop::vm::VM> s_pVM;
static std::string s_sLastOutput;

const std::string& bloop::test::TEST_LastOutput() {
	return s_sLastOutput;
}

bloop::vm::VM* bloop::test::TEST_GetLastVM() {
	return s_pVM.get();
}

std::optional<Value> bloop::test::TEST_ExecuteBuffer(bloop::BloopStringView buffer, const bloop::standard::Environment& environment) {
	// imports in a buffer are relative to tests/vm
	return Execute([&](bloop::metadata::Metadata& metadata) { return bloop::module::CompileSource(buffer, metadata, ScriptDirectory()); }, environment);
}

static std::optional<Value> Execute(const std::function<std::unique_ptr<bloop::ast::Program>(bloop::metadata::Metadata&)>& compile,
	const bloop::standard::Environment& environment) {

	s_sLastOutput.clear();

    try {
		bloop::metadata::Metadata metadata;

		if (const auto code = compile(metadata)) {
			s_pVM = std::make_unique<bloop::vm::VM>(metadata, environment);

			// keep whatever the script prints (std.console.log, runtime errors) out of the test log
			bloop::test::CoutCapture capture;
			auto ret = s_pVM->Run();
			s_sLastOutput = capture.Str();
			return ret;
		}
	} catch (std::runtime_error& ex) {
		FAIL(ex.what());
	}
	return std::nullopt;
}
