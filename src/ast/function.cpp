#include "ast/function.hpp"
#include "bytecode/defs.hpp"

using namespace bloop::ast;
#include <iostream>
void FunctionDeclarationStatement::EmitByteCode(TBCBuilder& parent) {

	TBCBuilder fnBuilder(parent.m_oAllFunctions);
	m_pBody->EmitByteCode(fnBuilder);

	fnBuilder.EnsureReturn(this);

	std::vector<bloop::bytecode::vmdata::Capture> c;
	c.reserve(m_oCaptures.size());

	for (auto& cap : m_oCaptures)
		c.emplace_back(cap.ToBC());

	PrintInstructions(fnBuilder);

	fnBuilder.m_oAllFunctions[m_uFunctionId] = {
		.m_sName = m_sName,
		.m_uParamCount = static_cast<bloop::BloopUInt16>(m_oParams.size()),
		.m_uLocalCount = m_uLocalCount,
		.chunk = fnBuilder.Finalize(),
		.m_oCaptures = c
	};

	parent.AddFunction(&fnBuilder.m_oAllFunctions[m_uFunctionId]);

	if (m_oCaptures.empty()) {
		return Emit(parent, TOpCode::MAKE_FUNCTION, m_uFunctionId);
	}

	Emit(parent, TOpCode::MAKE_CLOSURE, m_uFunctionId);

	for (auto& cap : m_oCaptures) {
		parent.EmitCapture({ cap.m_iDepth == 1, cap.m_uSlot }, m_oApproximatePosition);
	}

}

