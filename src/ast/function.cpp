#include "ast/function.hpp"
#include "bytecode/defs.hpp"

using namespace bloop::ast;
#include <iostream>
using ResolvedIdentifier = bloop::resolver::internal::ResolvedIdentifier;

inline static auto ConvertCaptures(const auto& captures) {
	std::vector<bloop::bytecode::vmdata::Capture> c;
	c.reserve(captures.size());

	for (auto& cap : captures)
		c.emplace_back(cap.ToBC());

	return c;
}

void FunctionDeclarationStatement::EmitByteCode(TBCBuilder& parent) {

	TBCBuilder fnBuilder(parent.m_oAllFunctions);

	m_pBody->EmitByteCode(fnBuilder);
	fnBuilder.EnsureReturn(this);
	PrintInstructions(fnBuilder);

	fnBuilder.m_oAllFunctions[m_uFunctionId] = {
		.m_sName = m_sName,
		.m_uParamCount = static_cast<bloop::BloopIndex>(m_oParams.size()),
		.m_uLocalCount = m_uLocalCount,
		.chunk = fnBuilder.Finalize(),
		.m_oCaptures = ConvertCaptures(m_oCaptures)
	};

	parent.AddFunction(&fnBuilder.m_oAllFunctions[m_uFunctionId]);

	if (m_oCaptures.empty()) {
		Emit(parent, TOpCode::MAKE_FUNCTION, m_uFunctionId);
	}
	else {
		Emit(parent, TOpCode::MAKE_CLOSURE, m_uFunctionId);

		for (auto& cap : m_oCaptures) {
			parent.EmitCapture({ cap.kind == Capture::Kind::Local, cap.m_uSlot }, m_oApproximatePosition);
		}
	}
	

	switch (m_oIdentifier.m_eKind) {
	case ResolvedIdentifier::Kind::Local:
		Emit(parent, TOpCode::STORE_LOCAL, m_oIdentifier.m_uSlot);
		break;
	case ResolvedIdentifier::Kind::Upvalue:
		Emit(parent, TOpCode::STORE_UPVALUE, m_oIdentifier.m_uSlot);
		break;
	case ResolvedIdentifier::Kind::Global:
		Emit(parent, TOpCode::STORE_GLOBAL, m_oIdentifier.m_uSlot);
		break;
	}


}

