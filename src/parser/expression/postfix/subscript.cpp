#include "parser/expression/postfix/subscript.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
#include "ast/postfix.hpp"

using namespace bloop::parser;

std::unique_ptr<IPostfix> CParserPostfix::ParseSubscript() {

	Advance(1); //skip [

	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_bracket_close)) {
		Advance(1); // skip ]
		return std::make_unique<CPostfixSubscript>();
	}

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_bracket_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return std::make_unique<CPostfixSubscript>(expr.ToExpression());
}

CPostfixSubscript::CPostfixSubscript(UniqueExpression&& index) :
	m_pIndex(std::forward<decltype(index)>(index)){}

std::unique_ptr<BinaryExpression> CPostfixSubscript::ToExpression() {
	return std::make_unique<bloop::ast::Subscript>(std::move(m_pIndex), m_oDeclPos);
}