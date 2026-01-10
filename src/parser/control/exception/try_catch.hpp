#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/control/control.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;

	namespace ast {
		struct ConstVariableDeclaration;
	}
}

namespace bloop::parser {
	struct CParserContext;

	class CParserTryCatchStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserTryCatchStatement);
	public:
		CParserTryCatchStatement() = delete;
		CParserTryCatchStatement(const CParserContext& ctx);
		~CParserTryCatchStatement() = default;

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		[[nodiscard]] bloop::EStatus ParseCatch();

		std::unique_ptr<bloop::ast::BlockStatement> m_pTryBlock;
		std::unique_ptr<bloop::ast::BlockStatement> m_pCatchBlock;
		std::unique_ptr<bloop::ast::ConstVariableDeclaration> m_pCatchVariable;
	};
}