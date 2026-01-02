#include "ast/ast.hpp"
#include "lexer/token.hpp"
#include "parser/declaration/declaration.hpp"
#include "parser/exception.hpp"
#include "parser/expression/expression.hpp"
#include "parser/parser.hpp"
#include "parser/scope/scope.hpp"
#include "parser/statements/for/for.hpp"

using namespace bloop::parser;

CParserForStatement::CParserForStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}
CParserForStatement::~CParserForStatement() = default;

bloop::EStatus CParserForStatement::Parse() {

	ParseIdentifier(bloop::ETokenType::tt_for);

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected a \"(\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); // skip (

	m_pBody = std::make_unique<bloop::ast::BlockStatement>(GetIteratorSafe()->GetCodePosition());
	auto oldBlock = m_oCtx.m_pCurrentBlock;
	m_oCtx.m_pCurrentBlock = m_pBody.get();

	if (ParseInitializer() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseCondition() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseEndExpression() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	CParserScope sc(m_oCtx);
	sc.ParseNoScope(true);

	m_oCtx.m_pCurrentBlock = oldBlock;
	return bloop::EStatus::success;

}
bloop::EStatus CParserForStatement::ParseInitializer() {

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected an expression"));

	//for(; ...
	if (GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_semicolon)) {
		Advance(1);
		return bloop::EStatus::success;
	}

	//for(const ...
	if (IsDeclaration(GetIteratorSafe())) {
		CParserDeclaration parser(m_oCtx);

		if (parser.Parse() != bloop::EStatus::success)
			return bloop::EStatus::failure;

		m_pInitializer = parser.ToStatement();
	} else {
		CParserExpression expr(m_oCtx);
		if (expr.Parse() != bloop::EStatus::success)
			return bloop::EStatus::failure;

		m_pInitializer = expr.ToStatement();
	}
	Advance(1); // skip ;
	return bloop::EStatus::success;
}
bloop::EStatus CParserForStatement::ParseCondition() {

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected an expression"));

	//for(... ; ...
	if (GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_semicolon)) {
		Advance(1);
		return bloop::EStatus::success;
	}

	CParserExpression expr(m_oCtx);

	if (expr.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_pCondition = expr.ToExpression();
	Advance(1); // skip ;
	return bloop::EStatus::success;
}
bloop::EStatus CParserForStatement::ParseEndExpression() {
	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected an expression"));

	//for ... ... )
	if (GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_close)) {
		Advance(1);
		return bloop::EStatus::success;
	}

	CParserExpression expr(m_oCtx);

	//find the )
	if (expr.Parse(PairMatcher(bloop::EPunctuation::p_par_open)) != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_pOnEnd = expr.ToExpression();
	return bloop::EStatus::success;
}
UniqueStatement CParserForStatement::ToStatement() {

	auto&& ptr = std::make_unique<bloop::ast::ForStatement>(m_oDeclPos);
	ptr->m_pInitializer = std::move(m_pInitializer);
	ptr->m_pCondition = std::move(m_pCondition);
	ptr->m_pOnEnd = std::move(m_pOnEnd);
	ptr->m_oStatements = std::move(m_pBody->m_oStatements);

	return ptr;
}