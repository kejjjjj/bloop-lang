#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>

namespace bloop::parser {
	struct CParserContext;
	struct CExpressionChain;

	struct COperator {
		const bloop::CPunctuationToken* m_pToken;
	};

	class COperatorParser final : public CParserSingle<CPunctuationToken> {
	public:
		COperatorParser() = delete;
		COperatorParser(const CParserContext& ctx);

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe, CExpressionChain* expression, EEvaluationType evalType);
		[[nodiscard]] auto GetToken() const { return m_pToken; }

	private:
		[[nodiscard]] bool CheckOperator() const;
		[[nodiscard]] bool IsOperator(const CPunctuationToken* token) const noexcept;
		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;
		[[nodiscard]] bloop::EStatus ParseSequence(std::optional<PairMatcher>& m_oEndOfExpression, CExpressionChain* expression);

		const CParserContext& m_oCtx;
	};

}