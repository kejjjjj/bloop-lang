#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;

	namespace ast {
		struct BlockStatement;
	}
}

namespace bloop::parser {
	struct CParserContext;

	class CParserStatement : public CParserSingle<bloop::CToken>, protected IStatement {
		BLOOP_NONCOPYABLE(CParserStatement);
	public:
		CParserStatement() = delete;
		CParserStatement(const CParserContext& ctx);
		virtual ~CParserStatement() = default;

		[[nodiscard]] virtual UniqueStatement ToStatement() override = 0;

	protected:
		void StartScope();
		void ParseIdentifier(bloop::ETokenType tt);
		[[nodiscard]] virtual UniqueExpression ParseExpression();
		[[nodiscard]] virtual UniqueStatement ParseStatement();

		virtual void ParseScope();
		[[nodiscard]] virtual std::unique_ptr<bloop::ast::BlockStatement> ParseScopeNormal();

		const CParserContext& m_oCtx;
		bloop::CodePosition m_oDeclPos;
		std::unique_ptr<bloop::ast::BlockStatement> m_pBody;
		bloop::ast::BlockStatement* m_pOldBlock{};
	};
}