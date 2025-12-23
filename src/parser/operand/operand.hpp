#pragma once

#include "utils/defs.hpp"
#include "parser/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>

namespace bloop::ast {
	struct Expression;
}

namespace bloop::parser {
	using ASTExpression = bloop::ast::Expression;

	struct CParserContext;
	enum class EOperandBaseType : char {
		ot_constant,
		ot_identifier
	};

	struct IOperand {
		friend class CParserOperand;
		BLOOP_NONCOPYABLE(IOperand);

		IOperand() = default;
		virtual ~IOperand() = default;

		[[nodiscard]] constexpr virtual EOperandBaseType Type() const noexcept = 0;
		[[nodiscard]] virtual std::unique_ptr<ASTExpression> ToExpression() = 0;
	protected:
		bloop::CodePosition m_oDeclPos;
	};

	class CParserOperand final : public CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserOperand);
	public:
		CParserOperand() = delete;
		CParserOperand(const CParserContext& ctx);
		~CParserOperand();

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe);

		constexpr auto& GetOperand() noexcept { return m_pOperand; }

	private:
		[[nodiscard]] std::unique_ptr<IOperand> ParseConstant();
		[[nodiscard]] std::unique_ptr<IOperand> ParseIdentifier();

		const CParserContext& m_oCtx;
		std::unique_ptr<IOperand> m_pOperand;
	};

}