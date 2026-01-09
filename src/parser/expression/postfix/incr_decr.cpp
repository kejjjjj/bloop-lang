#include "parser/expression/postfix/incr_decr.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
#include "ast/postfix.hpp"

using namespace bloop::parser;

std::unique_ptr<IPostfix> CParserPostfix::ParseIncrementDecrement() {

	const auto p = GetIteratorSafe()->GetPunctuation();
	assert(p && p->m_ePriority == bloop::EOperatorPriority::op_prefix);

	CPostfixIncrementDecrement::Type t{};

	switch (p->m_ePunctuation) {
	case bloop::EPunctuation::p_increment:
		t = CPostfixIncrementDecrement::Type::pf_increment;
		break;
	case bloop::EPunctuation::p_decrement:
		t = CPostfixIncrementDecrement::Type::pf_decrement;
		break;
	default:
		throw exception::ParserError(BLOOPTEXT("ParseIncrementDecrement() -> t == Type::pf_error"), p->GetCodePosition());
	}

	Advance(1); //skip ++/--
	return std::make_unique<CPostfixIncrementDecrement>(t);
}



std::unique_ptr<BinaryExpression> CPostfixIncrementDecrement::ToExpression() {

	switch (m_eType) {
	case CPostfixIncrementDecrement::Type::pf_increment:
		return std::make_unique<bloop::ast::Increment>(m_oDeclPos);
	case CPostfixIncrementDecrement::Type::pf_decrement:
		return std::make_unique<bloop::ast::Decrement>(m_oDeclPos);
	default:
		throw exception::ParserError(BLOOPTEXT("CPostfixIncrementDecrement::ToExpression()() -> t == Type::pf_error"), m_oDeclPos);

	}

}