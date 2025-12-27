#pragma once

#include "utils/defs.hpp"
#include "parser/defs.hpp"
#include "parser/expression/expression_context.hpp"

#include <optional>

namespace bloop::ast {
	struct Expression;
	struct BinaryExpression;
}

namespace bloop::parser {
	using ASTExpression = bloop::ast::Expression;
	using BinaryExpression = bloop::ast::BinaryExpression;

	struct CParserContext;

	template<typename T>
	struct IntfOperand {
		friend class CParserOperand;
		friend class CParserPostfix;

		BLOOP_NONCOPYABLE(IntfOperand);

		IntfOperand() = default;
		virtual ~IntfOperand() = default;

		[[nodiscard]] virtual std::unique_ptr<T> ToExpression() = 0;
	protected:
		bloop::CodePosition m_oDeclPos;
	};
	using IOperand = IntfOperand<ASTExpression>;
	using IPostfix = IntfOperand<BinaryExpression>;

	class CParserOperand final : public CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserOperand);
	public:
		CParserOperand() = delete;
		CParserOperand(const CParserContext& ctx);
		~CParserOperand();

		[[nodiscard]] bloop::EStatus Parse(std::optional<PairMatcher>& eoe);

		[[nodiscard]] constexpr auto& GetOperand() noexcept { return m_pOperand; }
		[[nodiscard]] std::unique_ptr<ASTExpression> ToExpression();

	private:
		[[nodiscard]] std::unique_ptr<IOperand> ParseConstant();
		[[nodiscard]] std::unique_ptr<IOperand> ParseIdentifier();
		[[nodiscard]] std::unique_ptr<IOperand> ParseParentheses();
		[[nodiscard]] std::unique_ptr<IOperand> ParseArray();

		[[nodiscard]] std::unique_ptr<BinaryExpression> PostfixesToAST() const noexcept;

		const CParserContext& m_oCtx;
		std::unique_ptr<IOperand> m_pOperand;
		std::vector<std::unique_ptr<IPostfix>> m_oPostfixes;

	};

}