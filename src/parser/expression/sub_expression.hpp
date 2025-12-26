#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>

/***********************************************************************
 > stores the operand on the lhs and the operator (if exists)
 > throws on failure
***********************************************************************/

namespace bloop::parser {
	struct CParserContext;
	struct COperator;
	struct CExpressionChain;

	class CParserOperand;

	class CParserSubExpression final : CParserSingle<bloop::CToken> {
		friend class CParserExpression;
		BLOOP_NONCOPYABLE(CParserSubExpression);
	public:
		CParserSubExpression() = delete;
		CParserSubExpression(const CParserContext& ctx);
		~CParserSubExpression();

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe, CExpressionChain* expression, EEvaluationType evalType);

	private:

		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		const CParserContext& m_oCtx;
		std::unique_ptr<CParserOperand> m_oLhsOperand;
		std::unique_ptr<COperator> m_oOperator;
	};
}