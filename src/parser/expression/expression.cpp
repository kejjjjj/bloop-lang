#include "parser/expression/expression.hpp"
#include "parser/parser.hpp"
#include "ast/ast.hpp"
#include "parser/exception.hpp"
#include "parser/expression/sub_expression.hpp"
#include "parser/operand/operand.hpp"
#include "lexer/punctuation.hpp"
#include "parser/expression/operator.hpp"

#include <cassert>
#include <iterator>

using namespace bloop::parser;

[[nodiscard]] static auto MakeBinary(bloop::EPunctuation punct, bloop::CodePosition pos) {
	return std::make_unique<bloop::ast::BinaryExpression>(punct, pos);
}

CExpressionChain::CExpressionChain() = default;
CExpressionChain::~CExpressionChain() {
	// unlink iteratively, a recursive destruction could overflow the stack on long lists
	auto next = std::move(m_pNext);
	while (next)
		next = std::move(next->m_pNext);
}

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

CParserExpressionStatement::CParserExpressionStatement(const CParserContext& ctx) : CParserExpression(ctx){}
CParserExpressionStatement::~CParserExpressionStatement() = default;

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

	// comma separated elements are parsed iteratively, each one with its own CParserExpression
	auto sequenceFollows = ParseElement(eoe, actualExpression, evalType);

	while (sequenceFollows) {
		actualExpression->m_pNext = std::make_unique<CExpressionChain>();
		actualExpression = actualExpression->m_pNext.get();

		CParserExpression next(m_oCtx);
		if (next.EndOfExpression(eoe))
			throw exception::ParserError(BLOOPTEXT("expected an expression"), next.GetIteratorSafe()->GetCodePosition());

		sequenceFollows = next.ParseElement(eoe, actualExpression, EEvaluationType::evaluate_everything);
	}

	return EStatus::success;
}
bool CParserExpression::ParseElement(std::optional<PairMatcher>& eoe, CExpressionChain* actualExpression, EEvaluationType evalType) {

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

	const auto sequenceFollows = m_oSubExpressions.back()->m_bSequenceFollows;
	actualExpression->m_pExpression = ToExpression_Internal();
	return sequenceFollows;

}

UniqueStatement CParserExpressionStatement::ToStatement() {
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
std::unique_ptr<bloop::ast::AssignExpression> CParserExpression::MakeAssignment(bloop::EPunctuation p, bloop::CodePosition pos) {
	return std::make_unique<bloop::ast::AssignExpression>(p, pos);
}
void CParserExpression::SetBranch(UniqueExpression& getter, COperator* op) {
	const auto* t = op->m_pToken;

	if (t->m_ePriority == bloop::EOperatorPriority::op_assignment) {
		getter = MakeAssignment(t->m_ePunctuation, t->GetCodePosition());
	} else if (t->IsOperator(bloop::EPunctuation::p_question_mark)) {
		assert(op->m_pThen);
		auto conditional = std::make_unique<bloop::ast::ConditionalExpression>(t->GetCodePosition());
		conditional->m_pThen = std::move(op->m_pThen);
		getter = std::move(conditional);
	} else {
		getter = MakeBinary(t->m_ePunctuation, t->GetCodePosition());
	}
}


UniqueExpression CParserExpression::CreateExpression(Operands& operands, Operators& operators) {


	if (auto&& leaf = GetLeaf(operands))
		return leaf;

	assert(!operators.empty());

	UniqueExpression oper;
	SetBranch(oper, *FindLowestPriorityOperator(operators));
	CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(oper.get()), operands, operators);
	return oper;
}
UniqueExpression CParserExpression::GetLeaf(Operands& operands) {
	if (operands.size() == 1)
		return operands.front()->ToExpression();
	return nullptr;
}
CParserExpression::Operators::iterator CParserExpression::FindLowestPriorityOperator(Operators& operators) {

	assert(!operators.empty());

	// among operators with the same priority:
	//  - assignments and ?: are right associative (a = b = c -> a = (b = c), a ? b : c ? d : e -> a ? b : (c ? d : e)),
	//    so the leftmost one becomes the root
	//  - everything else is left associative (a - b - c -> (a - b) - c), so the rightmost one becomes the root
	// (this has to go by the position in the expression, comparing the operator pointers depends on the heap layout)
	auto lowest = operators.begin();
	for (auto it = std::next(lowest); it != operators.end(); ++it) {
		const auto priority = (*it)->m_pToken->m_ePriority;
		const auto lowestPriority = (*lowest)->m_pToken->m_ePriority;

		const auto rightAssociative = priority == bloop::EOperatorPriority::op_assignment || priority == bloop::EOperatorPriority::op_conditional;
		if (priority < lowestPriority || (priority == lowestPriority && !rightAssociative))
			lowest = it;
	}
	return lowest;
}

void CParserExpression::CreateExpressionRecursively(bloop::ast::BinaryExpression* _this, Operands& operands, Operators& operators) {
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
			SetBranch(_this->left, *l);
			CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(_this->left.get()), lhsOperands, lhsOperators);
		}
	} if (!rhsOperands.empty()) {
		if (_this->right = GetLeaf(rhsOperands), !_this->right) {
			const auto l = FindLowestPriorityOperator(rhsOperators);
			SetBranch(_this->right, *l);
			CreateExpressionRecursively(dynamic_cast<bloop::ast::BinaryExpression*>(_this->right.get()), rhsOperands, rhsOperators);
		}
	}


}
