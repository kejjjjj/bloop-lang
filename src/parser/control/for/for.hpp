#pragma once


#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/control/control.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;
}

namespace bloop::parser {
	struct CParserContext;

	class CParserForStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserForStatement);
	public:
		CParserForStatement() = delete;
		CParserForStatement(const CParserContext& ctx);
		~CParserForStatement();

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;

	private:

		[[nodiscard]] bloop::EStatus ParseInitializer();
		[[nodiscard]] bloop::EStatus ParseCondition();
		[[nodiscard]] bloop::EStatus ParseEndExpression();

		UniqueStatement m_pInitializer;
		UniqueExpression m_pCondition;
		UniqueExpression m_pOnEnd;
		std::unique_ptr<bloop::ast::BlockStatement> m_pBody;
	};
}