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

#include <memory>
#include <fstream>
#include <filesystem>
#include <iostream>

using namespace bloop::vm;

bloop::BloopString LoadScript(bloop::BloopString path) {
    std::ifstream file(path);
    if (!file) 
		FAIL(bloop::fmt::format(BLOOPTEXT("failed to open script: \"{}\""), path));
    return bloop::BloopString((std::istreambuf_iterator<bloop::BloopChar>(file)), std::istreambuf_iterator<bloop::BloopChar>());
}

std::optional<Value> bloop::test::TEST_ExecuteFile(const bloop::BloopString& relative_path) {

	const auto rel_path = std::filesystem::path("vm") / relative_path;
	std::cout << rel_path << '\n';
	const auto this_path = std::filesystem::path(SOURCE_DIR) / rel_path;
	return TEST_ExecuteBuffer(LoadScript(this_path.string()));
}

std::optional<Value> bloop::test::TEST_ExecuteBuffer(bloop::BloopStringView buffer) {

    try {
		bloop::metadata::Metadata metadata;
		auto lex = bloop::lexer::CLexer(buffer, metadata);
		lex.Parse();
			
		bloop::parser::CLexParser parser(lex);

		if (const auto code = parser.Parse()) {
			bloop::resolver::Resolve(code.get(), metadata);
			bloop::bytecode::BuildByteCode(code.get(), metadata);

			//silly, but it doesn't destroy the vm
			static std::unique_ptr<bloop::vm::VM> vm;
			vm = std::make_unique<bloop::vm::VM>(metadata);

			return vm->Run(BLOOPTEXT("main"));
		}
	} catch (std::runtime_error& ex) {
		FAIL(ex.what());
	}
	return std::nullopt;
}