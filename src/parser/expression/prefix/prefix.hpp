#pragma once

#include "utils/defs.hpp"
#include "parser/defs.hpp"
#include "parser/operand/operand.hpp"

namespace bloop::parser {
	struct CParserContext;

	struct CPrefixOperation final : IPostfix {

		enum class Type {
			o_error,
			o_negation,
			o_increment,
			o_decrement
		};

		CPrefixOperation() = default;
		CPrefixOperation(Type t) : m_eType(t) {};
		[[nodiscard]] std::unique_ptr<BinaryExpression> ToExpression() override;
	private:
		Type m_eType{};
	};

	class CParserPrefix final : public CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserPrefix);
	public:
		CParserPrefix() = delete;
		CParserPrefix(const CParserContext& ctx);
		~CParserPrefix();

		[[nodiscard]] bloop::EStatus Parse();

		constexpr auto&& GetPrefixes() noexcept { return std::move(m_oPrefixes); }

	private:
		[[nodiscard]] bool IsPrefixOperator(const CPunctuationToken* token) const noexcept;

		[[nodiscard]] std::unique_ptr<IPrefix> ParseNegation();

		const CParserContext& m_oCtx;
		std::vector<std::unique_ptr<IPrefix>> m_oPrefixes;
	};

}