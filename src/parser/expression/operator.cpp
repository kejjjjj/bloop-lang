#include "operator.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"

using namespace bloop::parser;

COperatorParser::COperatorParser(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}

bloop::EStatus COperatorParser::Parse(std::optional<PairMatcher>& eoe, [[maybe_unused]] EEvaluationType evalType) {

	if (EndOfExpression(eoe))
		return EStatus::failure;

	if (IsEndOfBuffer() || !CheckOperator())
		throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());

	m_pToken = GetIteratorSafe()->GetPunctuation();

	//if (!IsOperator(m_pToken)) {
	//	throw exception::ParserError(BLOOPTEXT("unexpected end of expression: ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());
	//}

	Advance(1); //skip the operator
	return EStatus::success;
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