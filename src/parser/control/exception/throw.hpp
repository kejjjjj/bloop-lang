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

	class CParserThrowStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserThrowStatement);
	public:
		CParserThrowStatement() = delete;
		CParserThrowStatement(const CParserContext& ctx);
		~CParserThrowStatement() = default;

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueExpression ParseExpression() override;

		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		UniqueExpression m_pExpression;
	};
}