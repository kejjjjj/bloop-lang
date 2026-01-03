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

std::unique_ptr<bloop::ast::UnnamedScopeStatement> CParserScope::Parse(bool allowSingleStatement) {

	const auto [ iterPos, isCurlyBracket ] = ParseFirstPart(allowSingleStatement);

	auto block = std::make_unique<bloop::ast::UnnamedScopeStatement>(iterPos);
	auto oldBlock = m_oCtx.m_pCurrentBlock;
	m_oCtx.m_pCurrentBlock = block.get();

	ParseSecondPart(allowSingleStatement, isCurlyBracket);
	m_oCtx.m_pCurrentBlock = oldBlock;
	return block;
}
void CParserScope::ParseNoScope(bool allowSingleStatement) {
	const auto [_ ,isCurlyBracket] = ParseFirstPart(allowSingleStatement);
	ParseSecondPart(allowSingleStatement, isCurlyBracket);
}
CParserScope::FirstData CParserScope::ParseFirstPart([[maybe_unused]] bool allowSingleStatement) {
	assert(!IsEndOfBuffer() && (allowSingleStatement || GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_open)));
	const auto iterPos = GetIteratorSafe()->GetCodePosition();

	const auto isCurlyBracket = GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_open);

	if (isCurlyBracket) {
		Advance(1); // skip {

		if (GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_close))
			throw exception::ParserError(BLOOPTEXT("empty scopes aren't allowed"), GetIteratorSafe()->GetCodePosition());
	}

	return { iterPos, isCurlyBracket };
}
void CParserScope::ParseSecondPart(bool allowSingleStatement, bool isCurlyBracket)
{
	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a statement"), GetIteratorSafe()->GetCodePosition());

	if (allowSingleStatement && !isCurlyBracket) { // if() statement 
		[[maybe_unused]] const auto _ = ParseToken(m_oCtx);
	}
	else { //if() { control }
		do {
			if (ParseToken(m_oCtx) != bloop::EStatus::success)
				break;
			Advance(1);
		} while (!IsEndOfBuffer() && !GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_close));
	}

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a \"}\""), GetIteratorSafe()->GetCodePosition());
}