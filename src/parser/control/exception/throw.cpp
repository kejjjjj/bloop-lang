#include "parser/control/exception/throw.hpp"
#include "parser/parser.hpp"
#include "ast/exception.hpp"
#include "lexer/token.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"

using namespace bloop::parser;

CParserThrowStatement::CParserThrowStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}

bloop::EStatus CParserThrowStatement::Parse() {
	ParseIdentifier(bloop::ETokenType::tt_throw);

	//throw;
	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_semicolon))
		throw exception::ParserError(BLOOPTEXT("expected an expression"), GetIteratorSafe()->GetCodePosition());

	m_pExpression = ParseExpression();
	return bloop::EStatus::success;
}
UniqueExpression CParserThrowStatement::ParseExpression() {
	CParserExpression expr(m_oCtx);

	if (expr.Parse() != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return expr.ToExpression();
}
UniqueStatement CParserThrowStatement::ToStatement() {
	assert(m_pExpression);
	return std::make_unique<bloop::ast::ThrowStatement>(std::move(m_pExpression), m_oDeclPos);
}