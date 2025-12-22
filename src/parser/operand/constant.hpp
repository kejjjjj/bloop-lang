#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"

namespace bloop {
	class CToken;
}

namespace bloop::parser {

	struct CConstantOperand final : public IOperand {
		CConstantOperand(const bloop::CToken* token) : m_pToken(token) {}

		[[nodiscard]] constexpr EOperandBaseType Type() const noexcept override {
			return EOperandBaseType::ot_constant;
		}

		[[nodiscard]] std::unique_ptr<ASTExpression> ToExpression() override;


	private:

		[[nodiscard]] bloop::EValueType GetType() const noexcept;
		[[nodiscard]] std::size_t GetSize() const noexcept;
		[[nodiscard]] bloop::BloopString ToData() const noexcept;

		const bloop::CToken* m_pToken{};
	};

}