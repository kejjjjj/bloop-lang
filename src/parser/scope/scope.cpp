#include "parser/scope/scope.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "lexer/token.hpp"
#include "ast/ast.hpp"

#include <cassert>

using namespace bloop::parser;

CParserScope::CParserScope(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserScope::~CParserScope() = default;

std::unique_ptr<bloop::ast::BlockStatement> CParserScope::Parse() {

	assert(!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_open));
	Advance(1); // skip {

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_close))
		throw exception::ParserError(BLOOPTEXT("empty scopes aren't allowed"), GetIteratorSafe()->GetCodePosition());

	auto block = std::make_unique<bloop::ast::BlockStatement>(GetIteratorSafe()->GetCodePosition());
	auto oldBlock = m_oCtx.m_pCurrentBlock;

	m_oCtx.m_pCurrentBlock = block.get();

	if(IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a statement"), GetIteratorSafe()->GetCodePosition());

	do {
		if (ParseToken(m_oCtx) != bloop::EStatus::success)
			break;
		Advance(1);
	} while (!IsEndOfBuffer() && !GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_close));

	if(IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a \"}\""), GetIteratorSafe()->GetCodePosition());

	//Advance(1); //skip }

	m_oCtx.m_pCurrentBlock = oldBlock;
	return block;
}