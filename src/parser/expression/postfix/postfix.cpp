#include "parser/expression/postfix/postfix.hpp"
#include "parser/operand/operand.hpp"
#include "parser/parser.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"

#include <cassert>

using namespace bloop::parser;

CParserPostfix::CParserPostfix(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserPostfix::~CParserPostfix() = default;

bloop::EStatus CParserPostfix::Parse() {

	while (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator()) {
		const auto punct = GetIteratorSafe()->GetPunctuation();
		assert(punct);

		if (!IsPostfixOperator(punct))
			break;

		switch (punct->m_ePunctuation) {
		case EPunctuation::p_par_open:
			m_oPostfixes.emplace_back(ParseFunctionCall());
			break;
		}

	}

	std::ranges::reverse(m_oPostfixes);
	return bloop::EStatus::success;
}
bool CParserPostfix::IsPostfixOperator(const CPunctuationToken* token) const noexcept
{
	return
		token->m_ePriority == EOperatorPriority::op_postfix ||
		token->m_ePunctuation == EPunctuation::p_increment ||
		token->m_ePunctuation == EPunctuation::p_decrement;
}

