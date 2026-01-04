#pragma once

#include "parser/expression/postfix/postfix.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {

	struct CPostfixIncrementDecrement final : public IPostfix {

		enum class Type : bloop::BloopByte {
			pf_error,
			pf_increment,
			pf_decrement
		};

		CPostfixIncrementDecrement(Type t) : m_eType(t) {}
		~CPostfixIncrementDecrement() = default;

		[[nodiscard]] std::unique_ptr<BinaryExpression> ToExpression() override;
	private:
		Type m_eType{};
	};

}