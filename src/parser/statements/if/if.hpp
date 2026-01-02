#pragma once


#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/statements/statement.hpp"
#include "ast/ast.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;
}

namespace bloop::parser {
	struct CParserContext;
	using Structure = bloop::ast::IfStatement::Structure;

	class CParserIfStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserIfStatement);
	public:
		CParserIfStatement() = delete;
		CParserIfStatement(const CParserContext& ctx);
		~CParserIfStatement();

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		std::vector<std::unique_ptr<Structure>> m_oIf;
		std::unique_ptr<bloop::ast::BlockStatement> m_pElse;

	};
}