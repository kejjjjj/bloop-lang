#include "parser/operand/array.hpp"
#include "ast/ast.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
using namespace bloop::parser;

std::unique_ptr<IOperand> CParserOperand::ParseArray() {

	Advance(1); // skip [

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_bracket_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return std::make_unique<CArrayOperand>(expr.ToList());
}

CArrayOperand::CArrayOperand(std::vector<UniqueExpression>&& expr)
	: m_pInitializers(std::forward<decltype(expr)>(expr)){}
CArrayOperand::~CArrayOperand() = default;

UniqueExpression CArrayOperand::ToExpression() {
	return std::make_unique<bloop::ast::ArrayExpression>(std::move(m_pInitializers), m_oDeclPos);
}