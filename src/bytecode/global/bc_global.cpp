#include "bytecode/global/bc_global.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/function.hpp"
#include "std/native.hpp"

#include <iostream>

using namespace bloop::bytecode;

CByteCodeGlobals::CByteCodeGlobals(bloop::ast::Program* code)
	: m_pCode(code) {}
bloop::bc::Chunk CByteCodeGlobals::Generate(bloop::metadata::Metadata& metadata) {

	CByteCodeBuilder builder(metadata);

	//for (auto& func : m_pCode->m_oAllFunctions) {
	//	assert(func->m_oIdentifier.m_eKind != bloop::resolver::internal::ResolvedIdentifier::Kind::Error);
	//	builder.Emit(EOpCode::MAKE_FUNCTION, func->m_uFunctionId, func->m_oApproximatePosition);
	//	builder.Emit(EOpCode::STORE_GLOBAL, func->m_oIdentifier.m_uSlot, func->m_oApproximatePosition);
	//}

	for (auto& stmt : m_pCode->m_oStatements) {

		if (stmt->IsFunction()) {
			auto func = dynamic_cast<bloop::ast::FunctionDeclarationStatement*>(stmt.get());
			assert(func->m_oIdentifier.m_eKind != bloop::resolver::internal::ResolvedIdentifier::Kind::Error);
			stmt->Emit(builder, EOpCode::MAKE_FUNCTION, func->m_uFunctionId);
			stmt->Emit(builder, EOpCode::STORE_GLOBAL, func->m_oIdentifier.m_uSlot);
			continue;
		}

	}

	for (auto& stmt : m_pCode->m_oStatements) {

		if (stmt->IsFunction()) {
			continue;
		}

		stmt->EmitByteCode(builder);

	}

	#ifndef BLOOP_TEST
	std::cout << "\nglobal:\n";
	builder.Print();
	#endif
	return { 
		.m_oConstants = builder.m_oConstants, 
		.m_oByteCode = builder.Encode(),
		.m_oInstructions = builder.GetCodePositions()
	};
}