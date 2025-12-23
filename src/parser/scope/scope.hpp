#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"

#include <vector>
#include <memory>

namespace bloop::ast {
	struct BlockStatement;
}

namespace bloop::parser {
	struct CParserContext;

	class CParserScope final : CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserScope);
	public:
		CParserScope() = delete;
		CParserScope(const CParserContext& ctx);
		~CParserScope();

		[[nodiscard]] std::unique_ptr<bloop::ast::BlockStatement> Parse();

	private:
		const CParserContext& m_oCtx;

	};

}