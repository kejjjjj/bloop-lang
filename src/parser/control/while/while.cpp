#include "parser/control/while/while.hpp"
#include "parser/parser.hpp"
#include "ast/control.hpp"
#include "lexer/token.hpp"

using namespace bloop::parser;

CParserWhileStatement::CParserWhileStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}

bloop::EStatus CParserWhileStatement::Parse() {
	ParseIdentifier(bloop::ETokenType::tt_while);
	m_pCondition = ParseStatement();
	ParseScope();
	return (m_pCondition && m_pBody) ? bloop::EStatus::success : bloop::EStatus::failure;
}
UniqueStatement CParserWhileStatement::ToStatement() {

	auto&& whileStatement = std::make_unique<bloop::ast::WhileStatement>(m_oDeclPos);
	whileStatement->m_pCondition = std::move(m_pCondition);
	whileStatement->m_oStatements = std::move(m_pBody->m_oStatements);
	return whileStatement;
}