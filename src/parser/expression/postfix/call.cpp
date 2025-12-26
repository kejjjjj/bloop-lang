#include "parser/expression/postfix/call.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
#include "ast/postfix.hpp"

using namespace bloop::parser;

std::unique_ptr<IPostfix> CParserPostfix::ParseFunctionCall() {

	Advance(1); //skip (

	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_par_close)) {
		Advance(1); // skip )
		return std::make_unique<CPostfixFunctionCall>();
	}

	CParserExpression expr(m_oCtx);

	if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success)
		throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());

	return std::make_unique<CPostfixFunctionCall>(expr.ToList());
}

CPostfixFunctionCall::CPostfixFunctionCall(std::vector<UniqueExpression>&& args) : 
	m_oArgs(std::forward<decltype(args)>(args)){}

std::unique_ptr<BinaryExpression> CPostfixFunctionCall::ToExpression() {
	return std::make_unique<bloop::ast::FunctionCall>(std::move(m_oArgs), m_oDeclPos);
}