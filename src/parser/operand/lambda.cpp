#include "parser/operand/lambda.hpp"
#include "parser/expression/expression.hpp"
#include "parser/defs.hpp"
#include "parser/function/function.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "lexer/token.hpp"
#include "ast/ast.hpp"
#include "ast/control.hpp"
#include "ast/function.hpp"

using namespace bloop::parser;

CLambdaOperand::CLambdaOperand(UniqueExpression&& expr)
	: m_pExpression(std::forward<decltype(expr)>(expr)){}
CLambdaOperand::~CLambdaOperand() = default;

UniqueExpression CLambdaOperand::ToExpression() {
	assert(m_pExpression);
	return std::move(m_pExpression);
}

CLambdaChecker::CLambdaChecker(const CParserContext& ctx) 
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {}
CLambdaChecker::~CLambdaChecker() = default;

bloop::EStatus CLambdaChecker::Parse(std::optional<PairMatcher>& eoe) {
	bloop::parser::ParserIterator oldPos = m_iterPos;

	if (ParseInternal(eoe) != bloop::EStatus::success) {
		m_iterPos = oldPos; //rewind
		return bloop::EStatus::failure;
	}

	return bloop::EStatus::success;
}
bloop::EStatus CLambdaChecker::ParseInternal(std::optional<PairMatcher>& eoe) {

	if (EndOfExpression(eoe) || !GetIteratorSafe()->IsOperator(EPunctuation::p_par_open))
		return bloop::EStatus::failure;

	Advance(1); // skip (

	if (IsEndOfBuffer())
		return bloop::EStatus::failure;

	m_pFunction = std::make_unique<CParserFunction>(m_oCtx);
	m_pFunction->m_oDeclPos = (*Peek(-1))->GetCodePosition();

	if (m_oCtx.m_refMetadata.m_oParserData.m_uNumLambdas >= std::numeric_limits<bloop::BloopUInt>::max()) {
		throw exception::ParserError(
			bloop::fmt::format(BLOOPTEXT("more than {} lambda functions"), m_oCtx.m_refMetadata.m_oParserData.m_uNumLambdas),
			GetIteratorSafe()->GetCodePosition());
	}

	m_pFunction->m_sName = bloop::fmt::format(BLOOPTEXT("#lambda{}"), m_oCtx.m_refMetadata.m_oParserData.m_uNumLambdas++);

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_par_close)) {
		Advance(1); //skip params
	} else {
		bloop::parser::ParserIterator oldPos = m_iterPos - 1; //-1 because we need to return back to the (
		const auto result = HasParameters();
		m_iterPos = oldPos;

		if (!result)
			return bloop::EStatus::failure;

		if(m_pFunction->ParseParameters() != bloop::EStatus::success)
			return bloop::EStatus::failure;

	}

	if (!IsArrowFunction())
		return bloop::EStatus::failure; // just parentheses with a list of identifiers... ?

	Advance(2); //skip =>

	if (IsOneLiner()) {
		const bloop::parser::ParserIterator oldPos = m_iterPos;

		CParserExpression expr(m_oCtx);
		//I chose singular because of this ambiguity: f((a, b) => a + b, 1, 2)
		//problem: is the lambda expression "a + b" or "a + b, 1, 2"
		//the first option is a more common intent
		//more explicit intent would be better: f(((a, b) => a + b), 1, 2)
		if (expr.Parse(eoe, nullptr, EEvaluationType::evaluate_singular) != bloop::EStatus::success)
			return bloop::EStatus::failure;

		//add an implicit return statement
		m_pFunction->m_pBody = std::make_unique<bloop::ast::UnnamedScopeStatement>(GetIteratorSafe()->GetCodePosition());
		m_pFunction->m_pBody->AddStatement(std::make_unique<bloop::ast::ReturnStatement>(expr.ToExpression(), (*oldPos)->GetCodePosition()));

		//if (eoe)
		//	Advance(-1);

	} else {
		if (m_pFunction->ParseScope() != bloop::EStatus::success)
			return bloop::EStatus::failure;
		Advance(1); //skip }
	}

	m_pFunction->m_oEndPos = GetIteratorSafe()->GetCodePosition();

	return bloop::EStatus::success;
}
bool CLambdaChecker::HasParameters() const {
	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != bloop::ETokenType::tt_name)
		return false; // not an identifier, can't be a parameter

	Advance(1); //skip identifier

	if (IsEndOfBuffer())
		return false;

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_comma)) {
		Advance(1); //skip comma
		return HasParameters();
	}

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_par_close)) {
		Advance(1); //skip )
		return true;
	}

	return false; //something else
}

bool CLambdaChecker::IsArrowFunction() const noexcept {
	if (!CanPeek(2))
		return false;

	return GetIteratorSafe()->IsOperator(EPunctuation::p_assign) && (*Peek(1))->IsOperator(EPunctuation::p_greater_than);
}
bool CLambdaChecker::IsOneLiner() const noexcept {
	return !IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_open) == false;
}

bool CLambdaChecker::EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept {
	assert(!IsEndOfBuffer());

	if (!eoe)
		return (*m_iterPos)->IsOperator(EPunctuation::p_semicolon);

	if (!(*m_iterPos)->IsOperator())
		return false;

	return eoe->IsClosing(dynamic_cast<const CPunctuationToken*>(*m_iterPos)->m_ePunctuation);
}

UniqueExpression CLambdaChecker::ToExpression() {
	return std::make_unique<bloop::ast::FunctionExpression>(m_pFunction->ToActualStatement(), m_oDeclPos);
}