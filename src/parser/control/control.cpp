#include "parser/control/control.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "ast/ast.hpp"
#include "lexer/token.hpp"
#include "utils/fmt.hpp"
#include "parser/expression/expression.hpp"
#include "parser/scope/scope.hpp"
#include "parser/declaration/declaration.hpp"

using namespace bloop::parser;

CParserStatement::CParserStatement(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx){
	assert(!IsEndOfBuffer());
	m_oDeclPos = m_oCtx.GetIterator()->GetCodePosition();
}
void CParserStatement::StartScope() {
	m_pBody = std::make_unique<bloop::ast::BlockStatement>(GetIteratorSafe()->GetCodePosition());
	m_pOldBlock = m_oCtx.m_pCurrentBlock;
	m_oCtx.m_pCurrentBlock = m_pBody.get();
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

	StartScope();
	Advance(1); // skip (

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return expr.ToExpression();
}
UniqueStatement CParserStatement::ParseStatement() {

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected a \"(\""), GetIteratorSafe()->GetCodePosition());

	StartScope();
	Advance(1); // skip (

	//for(const ...
	if (IsDeclaration(GetIteratorSafe())) {
		CParserDeclaration parser(m_oCtx);

		if (parser.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
			return nullptr;

		return parser.ToStatement();
	}

	CParserExpressionStatement expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return expr.ToStatement();
}
void CParserStatement::ParseScope() {

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a \"{\" or a statement"), GetIteratorSafe()->GetCodePosition());

	CParserScope sc(m_oCtx);
	sc.ParseNoScope(true);
	assert(m_pOldBlock);
	m_oCtx.m_pCurrentBlock = m_pOldBlock;
	m_pOldBlock = nullptr;
}
std::unique_ptr<bloop::ast::BlockStatement> CParserStatement::ParseScopeNormal()
{
	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a \"{\" or a statement"), GetIteratorSafe()->GetCodePosition());

	CParserScope sc(m_oCtx);
	return sc.Parse(true);
}