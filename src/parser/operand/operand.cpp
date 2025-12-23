#include "parser/operand/operand.hpp"
#include "parser/parser.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/operand/constant.hpp"
#include <cassert>

using namespace bloop::parser;

CParserOperand::CParserOperand(const CParserContext& ctx) 
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserOperand::~CParserOperand() = default;

bloop::EStatus CParserOperand::Parse([[maybe_unused]]std::optional<PairMatcher>& eoe) {

	//todo: unary

	auto token = GetIteratorSafe();

	if (bloop::token::IsConstant(token->Type())) {
		m_pOperand = ParseConstant();
	} else if (token->Type() == ETokenType::tt_name) {
		m_pOperand = ParseIdentifier();
	} else {
		throw exception::ParserError(BLOOPTEXT("unsupported: ") + token->Source(), token->GetCodePosition());
	}

	//because it got overwritten
	m_pOperand->m_oDeclPos = token->GetCodePosition();


	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("unexpected end of buffer"), GetIteratorSafe()->GetCodePosition());

	//todo: postfix

	return EStatus::success;
}
