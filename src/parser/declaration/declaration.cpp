#include "parser/declaration/declaration.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "lexer/token.hpp"
#include "ast/ast.hpp"
#include "parser/expression/expression.hpp"

#include <cassert>

using namespace bloop::parser;

CParserDeclaration::CParserDeclaration(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserDeclaration::~CParserDeclaration() = default;

bloop::EStatus CParserDeclaration::Parse() {

	if (ParseIdentifier() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseInitializer() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	return bloop::EStatus::success;
}
bloop::EStatus CParserDeclaration::ParseIdentifier() {
	if (IsEndOfBuffer() || !IsDeclaration(GetIteratorSafe()))
		throw exception::ParserError(BLOOPTEXT("expected \"let\" or \"const\""), GetIteratorSafe()->GetCodePosition());

	m_bIsConst = GetIteratorSafe()->Type() == ETokenType::tt_const;

	Advance(1); //skip let/const

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != ETokenType::tt_name)
		throw exception::ParserError(BLOOPTEXT("expected an identifier"), GetIteratorSafe()->GetCodePosition());

	m_pIdentifier = GetIteratorSafe();
	Advance(1); //skip identifier
	return bloop::EStatus::success;
}
bloop::EStatus CParserDeclaration::ParseInitializer() {

	if(IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected \";\""), GetIteratorSafe()->GetCodePosition());

	//let var;
	if (GetIteratorSafe()->IsOperator(EPunctuation::p_semicolon))
		return bloop::EStatus::success;

	if(!GetIteratorSafe()->IsOperator(EPunctuation::p_assign))
		throw exception::ParserError(BLOOPTEXT("expected \";\" or \"=\""), GetIteratorSafe()->GetCodePosition());

	Advance(-1); // go back to the identifier to get the full expression

	CParserExpression expr(m_oCtx);

	if (expr.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_pExpression = expr.ToExpression();
	return bloop::EStatus::success;

}
UniqueStatement CParserDeclaration::ToStatement() {
	if (m_bIsConst)
		return std::make_unique<bloop::ast::ConstVariableDeclaration>(m_pIdentifier->Source(), ToExpression(), m_pIdentifier->GetCodePosition());

	return std::make_unique<bloop::ast::VariableDeclaration>(m_pIdentifier->Source(), ToExpression(), m_pIdentifier->GetCodePosition());
}
UniqueExpression CParserDeclaration::ToExpression() {
	assert(m_pExpression);
	return std::move(m_pExpression);
}
bool bloop::parser::IsDeclaration(const bloop::CToken* token) noexcept {
	return token && (token->Type() == bloop::ETokenType::tt_const || token->Type() == bloop::ETokenType::tt_let);
}