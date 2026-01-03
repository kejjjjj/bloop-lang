#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace bloop::ast {
	struct BinaryExpression;
	struct AssignExpression;
}

namespace bloop::parser {
	struct CParserContext;
	class CParserSubExpression;
	class CParserOperand;
	struct COperator;

	// when an expression contains a comma, this helper class will resolve them to a list of elements (fn call, array) or 
	// merge into a continuous AST
	struct CExpressionChain {
		BLOOP_NONCOPYABLE(CExpressionChain);

		CExpressionChain();
		~CExpressionChain();

		//(function calls, arrays, objects): returns all comma separated elements
		[[nodiscard]] std::vector<UniqueExpression> ToList();

		//(default): merges all comma separated expressions into one
		[[nodiscard]] UniqueExpression ToMerged();

		UniqueExpression m_pExpression;
		std::unique_ptr<CExpressionChain> m_pNext;
	};

	class CParserExpression : CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserExpression);
		using Operands = std::vector<CParserOperand*>;
		using Operators = std::vector<COperator*>;
	public:
		CParserExpression() = delete;
		CParserExpression(const CParserContext& ctx);
		virtual ~CParserExpression();

		[[nodiscard]] bloop::EStatus Parse(
			std::optional<PairMatcher> eoe = std::nullopt,
			CExpressionChain* expression = nullptr,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] bloop::EStatus ParseInternal(
			std::optional<PairMatcher>& eoe,
			CExpressionChain* expression = nullptr,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] UniqueExpression ToExpression();
		[[nodiscard]] std::vector<UniqueExpression> ToList();
	protected:
		[[nodiscard]] virtual constexpr bool IsStatement() const noexcept { return false; }
		bloop::CodePosition m_oDeclPos;
	private:
		[[nodiscard]] UniqueExpression ToExpression_Internal();

		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		const CParserContext& m_oCtx;
		std::vector<std::unique_ptr<CParserSubExpression>> m_oSubExpressions;
		std::unique_ptr<CExpressionChain> m_pEvaluatedExpressions;

		//EXPRESSION GENERATION
		[[nodiscard]] UniqueExpression CreateExpression(Operands& operands, Operators& operators);
		[[nodiscard]] UniqueExpression GetLeaf(Operands& operands);
		[[nodiscard]] Operators::iterator FindLowestPriorityOperator(Operators& operators);
		[[nodiscard]] void CreateExpressionRecursively(bloop::ast::BinaryExpression* _this, Operands& operands, Operators& operators);
		[[nodiscard]] std::unique_ptr<bloop::ast::AssignExpression> MakeAssignment(bloop::CodePosition pos);
		void SetBranch(UniqueExpression& getter, const bloop::CPunctuationToken* t);

	};

	class CParserExpressionStatement : public CParserExpression, protected IStatement {
		BLOOP_NONCOPYABLE(CParserExpressionStatement);
	public:
		CParserExpressionStatement() = delete;
		CParserExpressionStatement(const CParserContext& ctx);
		~CParserExpressionStatement();

		[[nodiscard]] UniqueStatement ToStatement() override;
		constexpr void MakeNotStatement() { m_bIsStatement = false; }
	private:
		[[nodiscard]] constexpr bool IsStatement() const noexcept override { return m_bIsStatement; }
		bool m_bIsStatement{ true };
	};
}