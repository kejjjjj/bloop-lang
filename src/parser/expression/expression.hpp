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

	class CParserExpression final : CParserSingle<bloop::CToken>, protected IStatement {
		BLOOP_NONCOPYABLE(CParserExpression);
	public:
		CParserExpression() = delete;
		CParserExpression(const CParserContext& ctx);
		~CParserExpression();

		[[nodiscard]] bloop::EStatus Parse(
			std::optional<PairMatcher> eoe = std::nullopt,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] bloop::EStatus ParseInternal(
			std::optional<PairMatcher>& eoe,
			EEvaluationType evalType = EEvaluationType::evaluate_everything);

		[[nodiscard]] UniqueStatement ToStatement() override;
		[[nodiscard]] UniqueExpression ToExpression();

	private:
		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		const CParserContext& m_oCtx;
		std::vector<std::unique_ptr<CParserSubExpression>> m_oSubExpressions;
		bloop::CodePosition m_oDeclPos;
	};
}