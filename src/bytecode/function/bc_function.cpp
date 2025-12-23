#include "bytecode/function/bc_function.hpp"
#include "bytecode/defs.hpp"
#include "bytecode/compile/emit.hpp"
#include "ast/ast.hpp"
using namespace bloop::bytecode;

CByteCodeFunction::CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl)
	: m_pFunc(funcDecl){}

void CByteCodeFunction::Generate() {

	CByteCodeBuilder b;
	m_pFunc->EmitByteCode(b);
	b.Print();
}