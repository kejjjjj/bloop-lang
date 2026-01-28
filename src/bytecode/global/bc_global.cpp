#include "bytecode/global/bc_global.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/function.hpp"
#include "vm/native/native.hpp"

#include <iostream>

using namespace bloop::bytecode;

CByteCodeGlobals::CByteCodeGlobals(bloop::ast::Program* code)
	: m_pCode(code) {}
vmdata::Chunk CByteCodeGlobals::Generate() {

	std::vector<vmdata::Function> unused;
	CByteCodeBuilderForGlobals builder(unused);

	builder.m_uNumGlobals += static_cast<bloop::BloopIndex>(bloop::vm::native::g_Natives.size());

	for (auto& stmt : m_pCode->m_oStatements) {

		if (stmt->IsFunction()) {
			auto func = dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get());
			stmt->Emit(builder, EOpCode::MAKE_FUNCTION, func->m_uFunctionId);
			stmt->Emit(builder, EOpCode::STORE_GLOBAL, func->m_oIdentifier.m_uSlot);
			builder.m_uNumGlobals++;
			continue;
		}

		if (stmt->IsDeclaration())
			builder.m_uNumGlobals++;
		
		stmt->EmitByteCode(builder);

	}

	#ifndef BLOOP_TEST
	std::cout << "\nglobal:\n";
	builder.Print();
	#endif
	return { 
		.m_oConstants = builder.m_oConstants, 
		.m_uNumGlobals = builder.m_uNumGlobals, 
		.m_oByteCode = builder.Encode(),
		.m_oPositions = builder.GetCodePositions()
	};
}