#include "parser/control/control_flow/control_flow.hpp"
#include "parser/parser.hpp"
#include "ast/control.hpp"
#include "lexer/token.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"

using namespace bloop::parser;

CParserControlStatement::CParserControlStatement(const CParserContext& ctx)
	: CParserStatement(ctx) {}

bloop::EStatus CParserControlStatement::Parse() {

	switch (GetIteratorSafe()->Type()) {
	case ETokenType::tt_continue:
		type = Type::cf_continue;
		break;
	case ETokenType::tt_break:
		type = Type::cf_break;
		break;
	default:
		type = Type::cf_error;
		break;
	}
	assert(type != Type::cf_error);

	Advance(1); // skip statement

	//return;
	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_semicolon))
		throw exception::ParserError(BLOOPTEXT("expected a \";\""), GetIteratorSafe()->GetCodePosition());

	return bloop::EStatus::success;
}

UniqueStatement CParserControlStatement::ToStatement() {
	switch (type) {
	case Type::cf_continue:
		return std::make_unique<bloop::ast::ContinueStatement>(m_oDeclPos);
	case Type::cf_break:
		return std::make_unique<bloop::ast::BreakStatement>(m_oDeclPos);
	default:
		throw exception::ParserError(BLOOPTEXT("type == Type::cf_error -> how?"), m_oDeclPos);
	}
}