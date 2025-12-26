#include "bytecode/global/bc_global.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/ast.hpp"

#include <iostream>

using namespace bloop::bytecode;

CByteCodeGlobals::CByteCodeGlobals(bloop::ast::Program* code)
	: m_pCode(code) {}
vmdata::Chunk CByteCodeGlobals::Generate() {

	CByteCodeBuilderForGlobals builder;
	for (auto& stmt : m_pCode->m_oStatements) {

		if (stmt->IsFunction()) {
			auto func = dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get());
			builder.Emit(EOpCode::MAKE_FUNCTION, func->m_uFunctionId);
			builder.Emit(EOpCode::DEFINE_GLOBAL, builder.m_uNumGlobals++);
			continue;
		}

		stmt->EmitByteCode(builder);

	}

	std::cout << "\nglobal:\n";
	builder.Print();

	return { 
		.m_oConstants = builder.m_oConstants, 
		.m_uNumGlobals = builder.m_uNumGlobals, 
		.m_oByteCode = builder.Encode() 
	};
}