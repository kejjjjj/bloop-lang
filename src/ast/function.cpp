#include "bytecode/defs.hpp"
#include "ast/function.hpp"

using namespace bloop::ast;
#include <iostream>
using ResolvedIdentifier = bloop::resolver::internal::ResolvedIdentifier;

void FunctionDeclarationStatement::EmitByteCode(TBCBuilder& parent) {

	TBCBuilder fnBuilder(parent.m_refMetadata);

	m_pBody->EmitByteCode(fnBuilder);
	fnBuilder.EnsureReturn(this);

	#ifndef BLOOP_TEST
	PrintInstructions(fnBuilder);
	#endif

	bloop::bc::Function f;
	f.m_oCaptures = m_oCaptures;
	f.m_uChunkIndex = parent.m_refMetadata.m_oVMData.AddChunk(fnBuilder.Finalize());
	f.m_uLocalCount = m_uLocalCount;
	f.m_uParamCount = static_cast<bloop::BloopIndex>(m_oParams.size());
	f.m_uId = m_uFunctionId;
	parent.m_refMetadata.m_oVMData.AddFunction(f, f.m_uId);
	parent.AddFunction(f.m_uId);

	EmitCaptures(parent);

	//normal functions implicitly store the identifier ( fn main() {} )
	//lambdas have already loaded the identifier ( main = () => {} ) 
	if(!parent.m_bIsLambda)
		EmitStoreIdentifier(parent);
}

void FunctionDeclarationStatement::EmitCaptures(TBCBuilder& parent) {
	if (m_oCaptures.empty()) {
		Emit(parent, TOpCode::MAKE_FUNCTION, m_uFunctionId);
	} else {
		Emit(parent, TOpCode::MAKE_CLOSURE, m_uFunctionId);

		for (const auto& cap : m_oCaptures) {
			parent.EmitCapture(cap, m_oApproximatePosition);
		}
	}
}
void FunctionDeclarationStatement::EmitStoreIdentifier(TBCBuilder& parent) {
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