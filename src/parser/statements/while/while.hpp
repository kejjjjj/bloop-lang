#pragma once


#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/statements/statement.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;
}

namespace bloop::parser {
	struct CParserContext;

	class CParserWhileStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserWhileStatement);
	public:
		CParserWhileStatement() = delete;
		CParserWhileStatement(const CParserContext& ctx);
		~CParserWhileStatement() = default;

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		UniqueExpression m_pCondition;
		UniqueStatement m_pBody;
	};
}