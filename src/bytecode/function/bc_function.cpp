#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/function.hpp"

#include <iostream>

using namespace bloop::bytecode;
CByteCodeFunction::CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl)
	: m_pFunc(funcDecl){}

// represents a global level function (depth = 0)
void CByteCodeFunction::Generate(std::vector<vmdata::Function>& funcs) {

	CByteCodeBuilder b(funcs);
	m_pFunc->EmitByteCode(b);

	//return vmdata::Function{
	//	.m_sName = m_pFunc->m_sName,
	//	.m_uParamCount = static_cast<bloop::BloopUInt16>(m_pFunc->m_oParams.size()),
	//	.m_uLocalCount = m_pFunc->m_uLocalCount,
	//	.chunk = b.Finalize()
	//};
}