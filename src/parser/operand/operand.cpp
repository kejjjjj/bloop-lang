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

	if (token->Type() >= ETokenType::tt_undefined && token->Type() <= ETokenType::tt_string) {
		m_pOperand = ParseConstant();
	} else {
		throw exception::ParserError(BLOOPTEXT("unsupported"), GetIteratorSafe()->GetCodePosition());
	}

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("unexpected end of buffer"), GetIteratorSafe()->GetCodePosition());

	//todo: postfix

	return EStatus::success;
}
