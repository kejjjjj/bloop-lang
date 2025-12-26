#pragma once

#include "parser/expression/postfix/postfix.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {

	struct CPostfixFunctionCall final : public IPostfix {
		CPostfixFunctionCall() = default;
		CPostfixFunctionCall(std::vector<UniqueExpression>&& args);
		[[nodiscard]] std::unique_ptr<ASTExpression> ToExpression() override;
	private:
		std::vector<UniqueExpression> m_oArgs;
	};

}