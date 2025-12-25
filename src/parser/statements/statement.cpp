#include "parser/statements/statement.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "ast/ast.hpp"
#include "lexer/token.hpp"
#include "utils/fmt.hpp"
#include "parser/expression/expression.hpp"
#include "parser/scope/scope.hpp"

using namespace bloop::parser;

CParserStatement::CParserStatement(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx){
	assert(!IsEndOfBuffer());
	m_oDeclPos = m_oCtx.GetIterator()->GetCodePosition();
}

void CParserStatement::ParseIdentifier(bloop::ETokenType tt) {

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != tt)
		throw exception::ParserError(bloop::fmt::format(BLOOPTEXT("expected \"{}\""), bloop::token::TokenName(tt)), 
			GetIteratorSafe()->GetCodePosition());

	Advance(1);
}
UniqueExpression CParserStatement::ParseExpression() {

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected a \"(\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); // skip (

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return expr.ToExpression();
}
UniqueStatement CParserStatement::ParseScope() {

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a \"{\" or a statement"), GetIteratorSafe()->GetCodePosition());

	CParserScope scope(m_oCtx);
	return scope.Parse(true);
}