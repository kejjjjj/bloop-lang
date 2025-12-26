#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace bloop::parser {
	struct CParserContext;
	class CParserSubExpression;

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

	class CParserExpression final : CParserSingle<bloop::CToken>, protected IStatement {
		BLOOP_NONCOPYABLE(CParserExpression);
	public:
		CParserExpression() = delete;
		CParserExpression(const CParserContext& ctx);
		~CParserExpression();

		[[nodiscard]] bloop::EStatus Parse(
			std::optional<PairMatcher> eoe = std::nullopt,
			CExpressionChain* expression = nullptr,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] bloop::EStatus ParseInternal(
			std::optional<PairMatcher>& eoe,
			CExpressionChain* expression = nullptr,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] UniqueStatement ToStatement() override;
		[[nodiscard]] UniqueExpression ToExpression();
		[[nodiscard]] std::vector<UniqueExpression> ToList();
	private:
		[[nodiscard]] UniqueExpression ToExpression_Internal();

		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		const CParserContext& m_oCtx;
		std::vector<std::unique_ptr<CParserSubExpression>> m_oSubExpressions;
		std::unique_ptr<CExpressionChain> m_pEvaluatedExpressions;
		bloop::CodePosition m_oDeclPos;
	};
}