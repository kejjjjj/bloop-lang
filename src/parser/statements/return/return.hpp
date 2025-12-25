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

	class CParserReturnStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserReturnStatement);
	public:
		CParserReturnStatement() = delete;
		CParserReturnStatement(const CParserContext& ctx);
		~CParserReturnStatement() = default;

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueExpression ParseExpression() override;

		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		UniqueExpression m_pExpression;
	};
}