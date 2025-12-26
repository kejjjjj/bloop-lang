#include "parser/operand/parentheses.hpp"
#include "ast/ast.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
using namespace bloop::parser;

std::unique_ptr<IOperand> CParserOperand::ParseParentheses() {

	Advance(1); // skip (

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return std::make_unique<CParenthesesOperand>(expr.ToExpression());
}

CParenthesesOperand::CParenthesesOperand(UniqueExpression&& expr) 
	: m_pExpression(std::forward<decltype(expr)>(expr)){}
CParenthesesOperand::~CParenthesesOperand() = default;

UniqueExpression CParenthesesOperand::ToExpression() {
	return std::move(m_pExpression);
}