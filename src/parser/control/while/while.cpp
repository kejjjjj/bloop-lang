#include "parser/control/while/while.hpp"
#include "parser/parser.hpp"
#include "ast/control.hpp"
#include "lexer/token.hpp"

using namespace bloop::parser;

CParserWhileStatement::CParserWhileStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}

bloop::EStatus CParserWhileStatement::Parse() {
	ParseIdentifier(bloop::ETokenType::tt_while);
	m_pCondition = ParseExpression();
	m_pBody = ParseScope();
	return (m_pCondition && m_pBody) ? bloop::EStatus::success : bloop::EStatus::failure;
}
UniqueStatement CParserWhileStatement::ToStatement() {

	auto&& whileStatement = std::make_unique<bloop::ast::WhileStatement>(m_oDeclPos);
	whileStatement->m_pCondition = std::move(m_pCondition);

	auto wtf = static_cast<bloop::ast::BlockStatement*>(m_pBody.get());
	whileStatement->m_oStatements = std::move(wtf->m_oStatements);
	return whileStatement;
}