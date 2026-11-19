#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>
#include <memory>
#include <utility>

namespace bloop::parser {
	struct CParserContext;
	struct CExpressionChain;

	struct COperator {
		~COperator(); // (where Expression is complete)

		const bloop::CPunctuationToken* m_pToken{};
		UniqueExpression m_pThen; // the middle of condition ? then : otherwise
	};

	class COperatorParser final : public CParserSingle<CPunctuationToken> {
	public:
		COperatorParser() = delete;
		COperatorParser(const CParserContext& ctx);

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe, CExpressionChain* expression, EEvaluationType evalType);
		[[nodiscard]] auto GetToken() const { return m_pToken; }
		[[nodiscard]] UniqueExpression TakeThen() noexcept { return std::move(m_pThen); }

		// a comma operator was consumed and another element of the sequence follows
		[[nodiscard]] bool SequenceFollows() const noexcept { return m_bSequenceFollows; }

	private:
		[[nodiscard]] bool CheckOperator() const;
		[[nodiscard]] bool IsOperator(const CPunctuationToken* token) const noexcept;
		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		[[nodiscard]] UniqueExpression ParseThen();

		bool m_bSequenceFollows{};
		UniqueExpression m_pThen;
		const CParserContext& m_oCtx;
	};

}