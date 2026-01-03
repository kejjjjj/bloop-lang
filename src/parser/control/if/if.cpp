#include "parser/control/if/if.hpp"
#include "parser/parser.hpp"
#include "ast/control.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"

using namespace bloop::parser;

CParserIfStatement::CParserIfStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}
CParserIfStatement::~CParserIfStatement() = default;

bloop::EStatus CParserIfStatement::Parse() {

	ParseIdentifier(bloop::ETokenType::tt_if);
	auto&& expr = ParseExpression();
	auto&& scope = ParseScope();
	m_oIf.emplace_back(std::make_unique<Structure>(Structure{ std::move(expr), std::move(scope) }));

	while(CanPeek(1) && (*Peek(1))->Type() == bloop::ETokenType::tt_else) { 
		
		Advance(1); //skip }

		if (m_pElse)
			throw exception::ParserError(BLOOPTEXT("the else block was already declared"), (*Peek(1))->GetCodePosition());

		//else if
		if (CanPeek(1) && (*Peek(1))->Type() == bloop::ETokenType::tt_if) {
			Advance(1); // skip else
			ParseIdentifier(bloop::ETokenType::tt_if);

			expr = ParseExpression();
			scope = ParseScope();
			m_oIf.emplace_back(std::make_unique<Structure>(Structure{ std::move(expr), std::move(scope) }));
			continue;
		}

		ParseIdentifier(bloop::ETokenType::tt_else);
		m_pElse = ParseScope();		
	}

	return bloop::EStatus::success;

}
UniqueStatement CParserIfStatement::ToStatement() {

	auto&& ptr = std::make_unique<bloop::ast::IfStatement>(m_oDeclPos);

	ptr->m_oIf = std::move(m_oIf);
	ptr->m_pElse = std::move(m_pElse);

	return ptr;
}