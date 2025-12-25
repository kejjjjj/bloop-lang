#include "bytecode/build.hpp"
#include "ast/ast.hpp"
#include "bytecode/function/bc_function.hpp"

using namespace bloop::bytecode;

std::vector<vmdata::Function> bloop::bytecode::BuildByteCode(bloop::ast::Program* code) {

	std::vector<vmdata::Function> functions;

	for (const auto& stmt : code->m_oStatements) {
		if (stmt->IsFunction()) {
			CByteCodeFunction f(dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get()));
			functions.push_back(f.Generate());
		}
	}

	return functions;

}