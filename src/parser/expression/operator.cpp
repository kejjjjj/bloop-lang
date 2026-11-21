#include "operator.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"
#include "parser/expression/expression.hpp"
#include "ast/ast.hpp"

using namespace bloop::parser;

COperator::~COperator() = default;

COperatorParser::COperatorParser(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
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

	// a ":" that isn't the end of an expression (object literals, ranged for) belongs to a "?"
	if (m_pToken->IsOperator(EPunctuation::p_colon))
		throw exception::ParserError(BLOOPTEXT("\":\" without a \"?\""), m_pToken->GetCodePosition());

	Advance(1); //skip the operator

	// condition ? then : otherwise
	// the middle part is parsed here as a whole expression, so the operator list only sees
	// condition [?: then] otherwise, and the "?:" operator splits it like any other binary operator
	if (m_pToken->IsOperator(EPunctuation::p_question_mark))
		m_pThen = ParseThen();

	return bloop::EStatus::success;
}
UniqueExpression COperatorParser::ParseThen() {
	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected an expression after \"?\""), m_pToken->GetCodePosition());

	// up to (and including) the matching ":". A nested "?" consumes its own ":" first
	CParserExpression then(m_oCtx);
	try {
		if (then.Parse(PairMatcher(EPunctuation::p_colon)) != bloop::EStatus::success)
			throw exception::ParserError(BLOOPTEXT("expected an expression after \"?\""), m_pToken->GetCodePosition());
	} catch (exception::ParserError&) {
		// the statement ended before the ":"
		if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_semicolon))
			throw exception::ParserError(BLOOPTEXT("expected a \":\" for this \"?\""), m_pToken->GetCodePosition());
		throw;
	}

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected an expression after \":\""), m_pToken->GetCodePosition());

	return then.ToExpression();
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

	return eoe->IsClosing((*m_iterPos)->GetPunctuation()->m_ePunctuation);
}