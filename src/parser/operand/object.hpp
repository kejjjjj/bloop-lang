#pragma once

#include "operand.hpp"
#include "utils/defs.hpp"
#include "ast/ast.hpp"

namespace bloop::parser {


	class CObjectOperand final : public IOperand {
		BLOOP_NONCOPYABLE(CObjectOperand);
		friend class CParserOperand;
	public:
		using KV = bloop::ast::ObjectExpression::KV;

		CObjectOperand() = default;
		CObjectOperand(std::vector<std::unique_ptr<KV>>&& kvs);
		~CObjectOperand();

		[[nodiscard]] UniqueExpression ToExpression() override;
	private:
		std::vector<std::unique_ptr<KV>> m_oKeyValues{};
	};

}