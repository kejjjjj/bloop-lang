#include "parser/control/exception/try_catch.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "ast/exception.hpp"
#include "lexer/token.hpp"

using namespace bloop::parser;

CParserTryCatchStatement::CParserTryCatchStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {
}

bloop::EStatus CParserTryCatchStatement::Parse() {
	ParseIdentifier(bloop::ETokenType::tt_try);
	
	m_pTryBlock = ParseScopeNormal();
	Advance(1); //skip }
	return ParseCatch();
}
bloop::EStatus CParserTryCatchStatement::ParseCatch() {

	ParseIdentifier(bloop::ETokenType::tt_catch);
	StartScope();

	if(IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected a \"(\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); //skip (

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != bloop::ETokenType::tt_name)
		throw exception::ParserError(BLOOPTEXT("expected an identifier"), GetIteratorSafe()->GetCodePosition());

	const auto idToken = GetIteratorSafe();
	m_pCatchVariable = std::make_unique<bloop::ast::ConstVariableDeclaration>(idToken->Source(), nullptr, idToken->GetCodePosition());

	Advance(1); //skip identifier

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_par_close))
		throw exception::ParserError(BLOOPTEXT("expected a \")\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); //skip )

	ParseScope();
	assert(m_pBody);
	m_pCatchBlock = std::move(m_pBody);

	return bloop::EStatus::success;
}
UniqueStatement CParserTryCatchStatement::ToStatement() {

	auto&& ptr = std::make_unique<bloop::ast::TryCatch>(m_oDeclPos);

	ptr->m_pTryBlock = std::move(m_pTryBlock);
	ptr->m_pCatchBlock = std::move(m_pCatchBlock);
	ptr->m_pCatchVariable = std::move(m_pCatchVariable);

	return ptr;
}