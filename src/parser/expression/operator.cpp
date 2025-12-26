#include "operator.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"
#include "parser/expression/expression.hpp"

using namespace bloop::parser;

COperatorParser::COperatorParser(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}

bloop::EStatus COperatorParser::Parse(std::optional<PairMatcher>& eoe, CExpressionChain* expression, EEvaluationType evalType) {

	if (EndOfExpression(eoe))
		return bloop::EStatus::failure;

	if (IsEndOfBuffer() || !CheckOperator())
		throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());

	m_pToken = GetIteratorSafe()->GetPunctuation();

	if (m_pToken->IsOperator(EPunctuation::p_comma)) {

		//we want to stop at the comma
		if (evalType == EEvaluationType::evaluate_singular)
			return bloop::EStatus::failure;

		if(ParseSequence(eoe, expression) != bloop::EStatus::success)
			return bloop::EStatus::failure;

	}

	if (!IsOperator(m_pToken)) {
		throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());
	}

	Advance(1); //skip the operator
	return bloop::EStatus::success;
}
bloop::EStatus COperatorParser::ParseSequence(std::optional<PairMatcher>& m_oEndOfExpression, CExpressionChain* expression) {

	// don't evaluate a sequence when parsing a list (for example arrays [1, 2, 3])
	const auto parsingList = m_oEndOfExpression && m_oEndOfExpression->IsClosing(EPunctuation::p_comma);

	if (parsingList)
		return bloop::EStatus::failure;

	Advance(1); // skip ,

	//prepare next item
	expression->m_pNext = std::make_unique<CExpressionChain>();

	auto nextExpr = CParserExpression(m_oCtx);
	if (nextExpr.Parse(m_oEndOfExpression, expression->m_pNext.get()) != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (m_oEndOfExpression && EndOfExpression(m_oEndOfExpression)) {
		Advance(1); // skip the closing character
		return bloop::EStatus::failure;
	}

	return bloop::EStatus::failure;
}
bool COperatorParser::CheckOperator() const {
	return (*m_iterPos)->IsOperator();
}
bool COperatorParser::IsOperator(const CPunctuationToken* token) const noexcept {
	return token->m_ePriority >= EOperatorPriority::op_assignment && token->m_ePriority <= EOperatorPriority::op_multiplicative;
}
bool COperatorParser::EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept {
	assert(!IsEndOfBuffer());

	if (!eoe)
		return (*m_iterPos)->IsOperator(EPunctuation::p_semicolon);

	if (!(*m_iterPos)->IsOperator())
		return false;

	return eoe->IsClosing(dynamic_cast<const CPunctuationToken*>(*m_iterPos)->m_ePunctuation);
}