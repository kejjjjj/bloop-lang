#include "parser/expression/prefix/prefix.hpp"
#include "parser/operand/operand.hpp"
#include "parser/parser.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "ast/prefix.hpp"

#include <cassert>

using namespace bloop::parser;

std::unique_ptr<BinaryExpression> CPrefixOperation::ToExpression()
{
	switch (m_eType) {
	case Type::o_negation:
		return std::make_unique<bloop::ast::Negation>(m_oDeclPos);
	case Type::o_increment:
		return std::make_unique<bloop::ast::PrefixIncrement>(m_oDeclPos);
	case Type::o_decrement:
		return std::make_unique<bloop::ast::PrefixDecrement>(m_oDeclPos);
	default:
		throw exception::ParserError(BLOOPTEXT("unknown prefix operation"), m_oDeclPos);
	}
}

CParserPrefix::CParserPrefix(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserPrefix::~CParserPrefix() = default;

bloop::EStatus CParserPrefix::Parse() {

	while (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator()) {
		const auto punct = GetIteratorSafe()->GetPunctuation();
		assert(punct);

		if (!IsPrefixOperator(punct))
			break;

		switch (punct->m_ePunctuation) {
		case EPunctuation::p_sub: 
			m_oPrefixes.emplace_back(std::make_unique<CPrefixOperation>(CPrefixOperation::Type::o_negation));
			break;
		case EPunctuation::p_increment: 
			m_oPrefixes.emplace_back(std::make_unique<CPrefixOperation>(CPrefixOperation::Type::o_increment));
			break;
		case EPunctuation::p_decrement: 
			m_oPrefixes.emplace_back(std::make_unique<CPrefixOperation>(CPrefixOperation::Type::o_decrement));
			break;
		default:
			throw exception::ParserError(BLOOPTEXT("unsupported postfix operator"), GetIteratorSafe()->GetCodePosition());
		}

		Advance(1);

		if(!m_oPrefixes.empty())
			m_oPrefixes.back()->m_oDeclPos = punct->GetCodePosition();

	}

	// no need to reverse here since they are evaluated from right to left
	return bloop::EStatus::success;
}
bool CParserPrefix::IsPrefixOperator(const CPunctuationToken* token) const noexcept {
	return token->m_ePriority == EOperatorPriority::op_prefix || token->m_ePunctuation == EPunctuation::p_sub;
}

