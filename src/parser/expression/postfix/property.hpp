#pragma once

#include "parser/expression/postfix/postfix.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {

	struct CPostfixPropertyAccess final : public IPostfix {

		CPostfixPropertyAccess(bloop::ConstantData t) : m_oName(t) {}
		~CPostfixPropertyAccess() = default;

		[[nodiscard]] std::unique_ptr<BinaryExpression> ToExpression() override;
	private:
		bloop::ConstantData m_oName;
	};

}