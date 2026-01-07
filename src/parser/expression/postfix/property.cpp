#include "parser/expression/postfix/property.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
#include "ast/postfix.hpp"
#include "parser/operand/constant.hpp"

using namespace bloop::parser;

std::unique_ptr<IPostfix> CParserPostfix::ParsePropertyAccess() {
	assert(GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_period));
	Advance(1); //skip .

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != bloop::ETokenType::tt_name)
		throw exception::ParserError(BLOOPTEXT("expected a property name"), GetIteratorSafe()->GetCodePosition());

	bloop::CToken t = *GetIteratorSafe();
	t.SetType(bloop::ETokenType::tt_string);

	Advance(1); //skip name

	return std::make_unique<CPostfixPropertyAccess>(CConstantOperand::FromToken(&t));
}

std::unique_ptr<BinaryExpression> CPostfixPropertyAccess::ToExpression() {
	return std::make_unique<bloop::ast::PropertyAccess>(m_oName, m_oDeclPos);
}