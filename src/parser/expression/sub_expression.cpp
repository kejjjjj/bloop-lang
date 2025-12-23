#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/expression/sub_expression.hpp"
#include "parser/operand/operand.hpp"
#include "parser/parser.hpp"
#include "parser/expression/operator.hpp"

#include <cassert>

using namespace bloop::parser;

CParserSubExpression::CParserSubExpression(const CParserContext& ctx) 
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {

	assert(!IsEndOfBuffer());
}
CParserSubExpression::~CParserSubExpression() = default;

bloop::EStatus CParserSubExpression::Parse(std::optional<PairMatcher>& eoe, EEvaluationType evalType) {

	//empty subexpression
	if (EndOfExpression(eoe))
		return EStatus::failure;

	m_oLhsOperand = std::make_unique<CParserOperand>(m_oCtx);
	if (m_oLhsOperand->Parse(eoe) != EStatus::success) {
		m_oLhsOperand.reset();
		return EStatus::failure;
	}

	//only one operand
	if (EndOfExpression(eoe))
		return EStatus::failure;

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("unexpected end of buffer"), GetIteratorSafe()->GetCodePosition());

	COperatorParser p(m_oCtx);

	if (p.Parse(eoe, evalType) != EStatus::success)
		return EStatus::failure;

	m_oOperator = std::make_unique<COperator>(COperator{ .m_pToken = p.GetToken() });
	return EStatus::success;
}

bool CParserSubExpression::EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept {
	if (IsEndOfBuffer())
		return false; // let it fail

	if (!eoe)
		return (*m_iterPos)->IsOperator(EPunctuation::p_semicolon);

	if (!(*m_iterPos)->IsOperator())
		return false;

	return eoe->IsClosing(dynamic_cast<const CPunctuationToken*>(*m_iterPos)->m_ePunctuation);
}
