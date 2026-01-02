#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"

#include <vector>
#include <memory>

namespace bloop{
	class CToken;
}

namespace bloop::ast {
	struct Expression;
}

namespace bloop::parser {
	struct CParserContext;

	class CParserDeclaration final : CParserSingle<bloop::CToken>, protected IStatement {
		BLOOP_NONCOPYABLE(CParserDeclaration);
	public:
		CParserDeclaration() = delete;
		CParserDeclaration(const CParserContext& ctx);
		~CParserDeclaration();

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;
		[[nodiscard]] UniqueExpression ToExpression();

	private:
		[[nodiscard]] bloop::EStatus ParseIdentifier();
		[[nodiscard]] bloop::EStatus ParseInitializer();

		const CParserContext& m_oCtx;
		bool m_bIsConst{};
		const bloop::CToken* m_pIdentifier{};
		std::unique_ptr<bloop::ast::Expression> m_pExpression;
	};

	[[nodiscard]] bool IsDeclaration(const bloop::CToken* token) noexcept;
}