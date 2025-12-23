#include "bytecode/build.hpp"
#include "ast/ast.hpp"
#include "bytecode/function/bc_function.hpp"

using namespace bloop::bytecode;

void bloop::bytecode::BuildByteCode(bloop::ast::Program* code) {

	for (const auto& stmt : code->m_oStatements) {

		if (stmt->IsFunction()) {
			CByteCodeFunction f(dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get()));
			f.Generate();
		}

	}



}