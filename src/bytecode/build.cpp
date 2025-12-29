#include "bytecode/build.hpp"
#include "ast/function.hpp"
#include "bytecode/function/bc_function.hpp"
#include "bytecode/global/bc_global.hpp"

using namespace bloop::bytecode;

VMByteCode bloop::bytecode::BuildByteCode(bloop::ast::Program* code) {

	CByteCodeGlobals globals(code);
	auto globalChunk = globals.Generate();

	std::vector<vmdata::Function> functions(code->m_uNumFunctions);

	for (const auto& stmt : code->m_oStatements) {
		if (stmt->IsFunction()) {
			CByteCodeFunction f(dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get()));
			f.Generate(functions);
		}
	}

	return { globalChunk, globalChunk.m_uNumGlobals, functions };

}