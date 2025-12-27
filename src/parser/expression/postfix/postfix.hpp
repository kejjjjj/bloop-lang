#pragma once

#include "utils/defs.hpp"
#include "parser/defs.hpp"
#include "parser/operand/operand.hpp"

namespace bloop::parser {
	struct CParserContext;

	class CParserPostfix final : public CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserPostfix);
	public:
		CParserPostfix() = delete;
		CParserPostfix(const CParserContext& ctx);
		~CParserPostfix();

		[[nodiscard]] bloop::EStatus Parse();

		constexpr auto&& GetPostfixes() noexcept { return std::move(m_oPostfixes); }

	private:
		[[nodiscard]] bool IsPostfixOperator(const CPunctuationToken* token) const noexcept;

		[[nodiscard]] std::unique_ptr<IPostfix> ParseFunctionCall();
		[[nodiscard]] std::unique_ptr<IPostfix> ParseSubscript();

		const CParserContext& m_oCtx;
		std::vector<std::unique_ptr<IPostfix>> m_oPostfixes;
	};

}