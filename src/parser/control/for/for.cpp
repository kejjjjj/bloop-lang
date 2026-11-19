#include "ast/control.hpp"
#include "lexer/token.hpp"
#include "parser/declaration/declaration.hpp"
#include "parser/exception.hpp"
#include "parser/expression/expression.hpp"
#include "parser/parser.hpp"
#include "parser/scope/scope.hpp"
#include "parser/control/for/for.hpp"

#include <cassert>

using namespace bloop::parser;

CParserForStatement::CParserForStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}
CParserForStatement::~CParserForStatement() = default;

bloop::EStatus CParserForStatement::Parse() {

	ParseIdentifier(bloop::ETokenType::tt_for);

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected a \"(\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); // skip (
	StartScope();

	// for (const x : iterable)
	if (IsRangedFor())
		return ParseRangedFor();

	if (ParseInitializer() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseCondition() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseEndExpression() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	ParseScope();
	return bloop::EStatus::success;
}
bool CParserForStatement::IsRangedFor() {

	// let/const name :
	if (IsEndOfBuffer() || !IsDeclaration(GetIteratorSafe()))
		return false;

	const auto* begin = GetIteratorSafe();
	auto steps = 0;
	auto result = false;

	Advance(1);
	steps++;
	if (!IsEndOfBuffer() && GetIteratorSafe()->Type() == bloop::ETokenType::tt_name) {
		Advance(1);
		steps++;
		result = !IsEndOfBuffer() && GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_colon);
	}

	Advance(-steps);
	assert(GetIteratorSafe() == begin);
	(void)begin;
	return result;
}
bloop::EStatus CParserForStatement::ParseRangedFor() {

	m_bRanged = true;
	m_bIsConst = GetIteratorSafe()->Type() == bloop::ETokenType::tt_const;
	Advance(1); // skip let/const

	m_pElementName = GetIteratorSafe();
	Advance(2); // skip the name and :

	if (IsEndOfBuffer() || GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_close))
		throw exception::ParserError(BLOOPTEXT("expected an expression after \":\""), GetIteratorSafe()->GetCodePosition());

	// the iterable, up to the )
	CParserExpression expr(m_oCtx);
	if (expr.Parse(PairMatcher(bloop::EPunctuation::p_par_open)) != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_pIterable = expr.ToExpression();

	ParseScope();
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
		CParserExpressionStatement expr(m_oCtx);
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

	CParserExpressionStatement expr(m_oCtx);

	//find the )
	if (expr.Parse(PairMatcher(bloop::EPunctuation::p_par_open)) != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_pOnEnd = std::make_unique<bloop::ast::ExpressionStatement>(expr.ToExpression(), m_oDeclPos);
	return bloop::EStatus::success;
}
UniqueStatement CParserForStatement::ToStatement() {

	if (m_bRanged) {
		auto&& ranged = std::make_unique<bloop::ast::RangedForStatement>(m_oDeclPos);
		ranged->m_sName = m_pElementName->Source();
		ranged->m_bIsConst = m_bIsConst;
		ranged->m_pIterable = std::move(m_pIterable);
		ranged->m_oStatements = std::move(m_pBody->m_oStatements);
		return ranged;
	}

	auto&& ptr = std::make_unique<bloop::ast::ForStatement>(m_oDeclPos);
	ptr->m_pInitializer = std::move(m_pInitializer);
	ptr->m_pCondition = std::move(m_pCondition);
	ptr->m_pOnEnd = std::move(m_pOnEnd);
	ptr->m_oStatements = std::move(m_pBody->m_oStatements);

	return ptr;
}