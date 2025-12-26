#include "parser/expression/expression.hpp"
#include "parser/parser.hpp"
#include "ast/ast.hpp"
#include "parser/exception.hpp"
#include "parser/expression/sub_expression.hpp"
#include "parser/operand/operand.hpp"
#include "lexer/punctuation.hpp"
#include "parser/expression/operator.hpp"

#include <cassert>

using namespace bloop::parser;

[[nodiscard]] static auto MakeBinary(bloop::EPunctuation punct, bloop::CodePosition pos) {
	return std::make_unique<bloop::ast::BinaryExpression>(punct, pos);
}

CExpressionChain::CExpressionChain() = default;
CExpressionChain::~CExpressionChain() = default;

std::vector<UniqueExpression> CExpressionChain::ToList() {
	
	std::vector<UniqueExpression> list;

	auto pos = this;
	while (pos) {
		list.emplace_back(std::move(pos->m_pExpression));
		pos = pos->m_pNext.get();
	}
	return list;
}
UniqueExpression CExpressionChain::ToMerged()
{
	if (!m_pNext)
		return std::move(m_pExpression);

	auto root = MakeBinary(bloop::EPunctuation::p_comma, m_pExpression->m_oApproximatePosition);
	root->left = std::move(m_pExpression);
	root->right = m_pNext->ToMerged();
	return root;
}
CParserExpression::CParserExpression(const CParserContext& ctx) 
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
	m_oDeclPos = GetIteratorSafe()->GetCodePosition();
}
CParserExpression::~CParserExpression() = default;

bloop::EStatus CParserExpression::Parse(std::optional<PairMatcher> eoe, CExpressionChain* expression, EEvaluationType evalType) {
	return ParseInternal(eoe, expression, evalType);
}

bloop::EStatus CParserExpression::ParseInternal(std::optional<PairMatcher>& eoe, CExpressionChain* expression, EEvaluationType evalType) {

	if (EndOfExpression(eoe))
		throw exception::ParserError(BLOOPTEXT("expected an expression"), GetIteratorSafe()->GetCodePosition());

	if (!expression) {
		m_pEvaluatedExpressions = std::make_unique<CExpressionChain>();
	}

	auto actualExpression = expression ? expression : m_pEvaluatedExpressions.get();
	auto status = EStatus::failure;

	do {
		//the previous token was an operator, so we need an operand
		if (EndOfExpression(eoe) && !m_oSubExpressions.empty())
			throw exception::ParserError(BLOOPTEXT("expected an operand, but found ") + GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition());

		auto subExpr = std::make_unique<CParserSubExpression>(m_oCtx);
		status = subExpr->Parse(eoe, actualExpression, evalType);
		m_oSubExpressions.emplace_back(std::move(subExpr));

	} while (status == EStatus::success);

	assert(!m_oSubExpressions.empty());

	if (evalType == EEvaluationType::evaluate_everything && eoe && EndOfExpression(eoe)) {
		Advance(1);
		eoe = std::nullopt;
	}

	actualExpression->m_pExpression = ToExpression_Internal();
	return EStatus::success;

}
using Operands = std::vector<CParserOperand*>;
using Operators = std::vector<COperator*>;
[[nodiscard]] static UniqueExpression CreateExpression(Operands& operands, Operators& operators);
UniqueStatement CParserExpression::ToStatement() {
	return std::make_unique<bloop::ast::ExpressionStatement>(ToExpression(), m_oDeclPos);
}
UniqueExpression CParserExpression::ToExpression_Internal() {

	Operands operands;
	Operators operators;

	for (const auto& subExpr : m_oSubExpressions) {
		operands.emplace_back(&*subExpr->m_oLhsOperand);
		if (subExpr->m_oOperator)
			operators.emplace_back(&*subExpr->m_oOperator);
	}

	assert(operands.size() == operators.size() + 1u);
	return CreateExpression(operands, operators);
}
UniqueExpression CParserExpression::ToExpression() {
	return m_pEvaluatedExpressions->ToMerged();
}
std::vector<UniqueExpression> CParserExpression::ToList() {
	assert(m_pEvaluatedExpressions);
	return m_pEvaluatedExpressions->ToList();
}
bool CParserExpression::EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept {
	assert(!IsEndOfBuffer());

	if (!eoe)
		return (*m_iterPos)->IsOperator(EPunctuation::p_semicolon);

	if (!(*m_iterPos)->IsOperator())
		return false;

	return eoe->IsClosing(dynamic_cast<const CPunctuationToken*>(*m_iterPos)->m_ePunctuation);
}

/* EXPRESSION GENERATION */
[[nodiscard]] static UniqueExpression GetLeaf(Operands& operands);
[[nodiscard]] static Operators::iterator FindLowestPriorityOperator(Operators& operators);
[[nodiscard]] static void CreateExpressionRecursively(bloop::ast::BinaryExpression* _this, Operands& operands, Operators& operators);

void SetBranch(UniqueExpression& getter, const bloop::CPunctuationToken* t) {
	if (t->m_ePunctuation == bloop::EPunctuation::p_assign)
		getter = MakeAssignment(t->GetCodePosition());
	else
		getter = MakeBinary(t->m_ePunctuation, t->GetCodePosition());
}

[[nodiscard]] static auto MakeAssignment(bloop::CodePosition pos) {
	return std::make_unique<bloop::ast::AssignExpression>(pos);
}
[[nodiscard]] static UniqueExpression CreateExpression(Operands& operands, Operators& operators) {


	if (auto&& leaf = GetLeaf(operands))
		return leaf;

	assert(!operators.empty());

	UniqueExpression oper;
	SetBranch(oper, (*FindLowestPriorityOperator(operators))->m_pToken);
	CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(oper.get()), operands, operators);
	return oper;
}
UniqueExpression GetLeaf(Operands& operands) {
	if (operands.size() == 1)
		return operands.front()->GetOperand()->ToExpression() //fix me
	return nullptr;
}
Operators::iterator FindLowestPriorityOperator(Operators& operators) {

	assert(!operators.empty());
	
	return std::min_element(operators.begin(), operators.end(),
		[](auto a, auto b) {
			return a->m_pToken->m_ePriority <= b->m_pToken->m_ePriority;
		}
	);
}

void CreateExpressionRecursively(bloop::ast::BinaryExpression* _this, Operands& operands, Operators& operators) {
	if (operands.empty()) {
		assert(operators.empty());
		return;
	}

	assert(!operands.empty() && !operators.empty());

	const auto itr1 = FindLowestPriorityOperator(operators);

	const Operators::iterator opLhs = itr1;
	const Operators::iterator opRhs = std::next(opLhs);

	const auto operandLhs = operands.begin() + std::distance(operators.begin(), itr1) + 1;
	const auto operandRhs = operands.begin() + std::distance(operators.begin(), itr1) + 1;

	auto lhsOperands = Operands(operands.begin(), operandLhs);
	auto rhsOperands = Operands(operandRhs, operands.end());

	auto lhsOperators = Operators(operators.begin(), opLhs);
	auto rhsOperators = Operators(opRhs, operators.end());

	if (!lhsOperands.empty()) {
		if (_this->left = GetLeaf(lhsOperands), !_this->left) {
			const auto l = FindLowestPriorityOperator(lhsOperators);
			SetBranch(_this->left, (*l)->m_pToken);
			CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(_this->left.get()), lhsOperands, lhsOperators);
		}
	} if (!rhsOperands.empty()) {
		if (_this->right = GetLeaf(rhsOperands), !_this->right) {
			const auto l = FindLowestPriorityOperator(rhsOperators);
			SetBranch(_this->right, (*l)->m_pToken);
			CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(_this->right.get()), rhsOperands, rhsOperators);
		}
	}


}
