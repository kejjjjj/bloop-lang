#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop::ast {
	struct FunctionExpression;
}

namespace bloop {
	class CToken;
}

namespace bloop::parser {

	struct CLambdaOperand final : public IOperand {
		BLOOP_NONCOPYABLE(CLambdaOperand);
		CLambdaOperand(UniqueExpression&& expr);
		~CLambdaOperand();

		[[nodiscard]] UniqueExpression ToExpression() override;
	private:
		UniqueExpression m_pExpression;
	};

	//lambda(= > ) ambiguity checker class
	//(args) => return 1;
	//(args) => {}
	struct CExpressionChain;
	class CParserFunction;
	class CLambdaChecker final : public IOperand, CParserSingle<bloop::CToken> {
	public:
		CLambdaChecker(const CParserContext& ctx);
		~CLambdaChecker();

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe);

		[[nodiscard]] UniqueExpression ToExpression() override;

	private:
		[[nodiscard]] bloop::EStatus ParseInternal(std::optional<PairMatcher>& eoe);
		[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept;

		[[nodiscard]] bool HasParameters() const;

		[[nodiscard]] bool IsArrowFunction() const noexcept;
		[[nodiscard]] bool IsOneLiner() const noexcept;

		const CParserContext& m_oCtx;
		std::unique_ptr<CParserFunction> m_pFunction;

	};

}