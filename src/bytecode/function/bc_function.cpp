#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/function.hpp"

#include <iostream>

using namespace bloop::bytecode;
CByteCodeFunction::CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl)
	: m_pFunc(funcDecl){}

// represents a global level function (depth = 0)
void CByteCodeFunction::Generate(bloop::metadata::Metadata& metadata) {

	CByteCodeBuilder b(metadata);

	m_pFunc->m_pBody->EmitByteCode(b);
	b.EnsureReturn(m_pFunc);

	#ifndef BLOOP_TEST
		m_pFunc->PrintInstructions(b);
	#endif
	
	bloop::bc::Function f;
	f.m_oCaptures = {};
	f.m_uChunkIndex = metadata.m_oVMData.AddChunk(b.Finalize());
	f.m_uLocalCount = m_pFunc->m_uLocalCount;
	f.m_uParamCount = static_cast<bloop::BloopIndex>(m_pFunc->m_oParams.size());
	f.m_uId = m_pFunc->m_uFunctionId;
	metadata.m_oVMData.AddFunction(f, f.m_uId);
	b.AddFunction(f.m_uId);
}