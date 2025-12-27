#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {
	struct CArrayOperand final : public IOperand {
		BLOOP_NONCOPYABLE(CArrayOperand);
		CArrayOperand(std::vector<UniqueExpression>&& initializers);
		~CArrayOperand();

		[[nodiscard]] UniqueExpression ToExpression() override;
	private:
		std::vector<UniqueExpression> m_pInitializers{};
	};

}