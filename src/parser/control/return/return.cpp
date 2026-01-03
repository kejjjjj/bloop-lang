#include "parser/control/return/return.hpp"
#include "parser/parser.hpp"
#include "ast/control.hpp"
#include "lexer/token.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"

using namespace bloop::parser;

CParserReturnStatement::CParserReturnStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}

bloop::EStatus CParserReturnStatement::Parse() {
	ParseIdentifier(bloop::ETokenType::tt_return);

	//return;
	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_semicolon))
		return bloop::EStatus::success;

	m_pExpression = ParseExpression();
	return bloop::EStatus::success;
}
UniqueExpression CParserReturnStatement::ParseExpression() {
	CParserExpression expr(m_oCtx);

	if (expr.Parse() != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return expr.ToExpression();
}
UniqueStatement CParserReturnStatement::ToStatement() {
	return std::make_unique<bloop::ast::ReturnStatement>(std::move(m_pExpression), m_oDeclPos);
}