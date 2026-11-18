#include "operator.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"
#include "parser/expression/expression.hpp"

using namespace bloop::parser;

COperatorParser::COperatorParser(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd) {
	assert(!IsEndOfBuffer());
}

bloop::EStatus COperatorParser::Parse(std::optional<PairMatcher>& eoe, [[maybe_unused]] CExpressionChain* expression, EEvaluationType evalType) {

	if (EndOfExpression(eoe))
		return bloop::EStatus::failure;

	if (IsEndOfBuffer() || !CheckOperator())
		throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());

	m_pToken = GetIteratorSafe()->GetPunctuation();

	if (m_pToken->IsOperator(EPunctuation::p_comma)) {

		//we want to stop at the comma
		if (evalType == EEvaluationType::evaluate_singular)
			return bloop::EStatus::failure;

		// don't evaluate a sequence when parsing a list (for example arrays [1, 2, 3])
		if (eoe && eoe->IsClosing(EPunctuation::p_comma))
			return bloop::EStatus::failure;

		// the caller (CParserExpression) continues with the next element in a loop.
		// (parsing it from here would recurse once per comma and overflow the stack on long lists)
		Advance(1); // skip ,
		m_bSequenceFollows = true;
		return bloop::EStatus::failure;
	}

	if (!IsOperator(m_pToken)) {
		throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());
	}

	Advance(1); //skip the operator
	return bloop::EStatus::success;
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