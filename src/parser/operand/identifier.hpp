#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop::parser {
	struct CIdentifierOperand final : public IOperand {
		CIdentifierOperand(const bloop::BloopString& name) : m_sName(name) {}

		[[nodiscard]] constexpr EOperandBaseType Type() const noexcept override {
			return EOperandBaseType::ot_identifier;
		}

		[[nodiscard]] std::unique_ptr<ASTExpression> ToExpression() override;
	private:
		bloop::BloopString m_sName{};
	};

}