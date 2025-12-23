#include "parser/operand/identifier.hpp"
#include "ast/ast.hpp"

using namespace bloop::parser;

std::unique_ptr<IOperand> CParserOperand::ParseIdentifier() {
	auto&& v = std::make_unique<CIdentifierOperand>((*m_iterPos)->Source());
	Advance(1);
	return v;
}

std::unique_ptr<ASTExpression> CIdentifierOperand::ToExpression(){
	auto&& ptr = std::make_unique<bloop::ast::IdentifierExpression>(m_oDeclPos);
	ptr->m_sName = m_sName;
	return ptr;
}