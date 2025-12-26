#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/ast.hpp"

#include <iostream>

using namespace bloop::bytecode;
CByteCodeFunction::CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl)
	: m_pFunc(funcDecl){}

vmdata::Function CByteCodeFunction::Generate() {

	CByteCodeBuilder b;
	m_pFunc->EmitByteCode(b);

	std::cout << '\n' << m_pFunc->m_sName << ":\n";
	b.Print();

	return vmdata::Function{
		.m_sName = m_pFunc->m_sName,
		.m_uParamCount = static_cast<bloop::BloopUInt16>(m_pFunc->m_oParams.size()),
		.m_uLocalCount = m_pFunc->m_uLocalCount,
		.chunk = {.m_oConstants = b.m_oConstants, .m_oByteCode = b.Encode()}
	};
}