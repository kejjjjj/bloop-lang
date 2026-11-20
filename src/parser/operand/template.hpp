#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {
	// `text ${expression} text`: the text pieces (string literals) and the substitutions, in order
	struct CTemplateOperand final : public IOperand {
		BLOOP_NONCOPYABLE(CTemplateOperand);
		CTemplateOperand(std::vector<UniqueExpression>&& parts);
		~CTemplateOperand();

		[[nodiscard]] UniqueExpression ToExpression() override;
	private:
		std::vector<UniqueExpression> m_oParts;
	};
}
