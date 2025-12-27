#pragma once

#include "parser/expression/postfix/postfix.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {

	struct CPostfixSubscript final : public IPostfix {
		CPostfixSubscript() = default;
		CPostfixSubscript(UniqueExpression&& index);
		[[nodiscard]] std::unique_ptr<BinaryExpression> ToExpression() override;
	private:
		UniqueExpression m_pIndex;
	};

}