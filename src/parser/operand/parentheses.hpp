#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {
	struct CParenthesesOperand final : public IOperand {
		BLOOP_NONCOPYABLE(CParenthesesOperand);
		CParenthesesOperand(UniqueExpression&& expr);
		~CParenthesesOperand();

		[[nodiscard]] UniqueExpression ToExpression() override;
	private:
		UniqueExpression m_pExpression{};
	};

}