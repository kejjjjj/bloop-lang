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

	class CParserFunction final : CParserSingle<bloop::CToken>, protected IStatement {
		BLOOP_NONCOPYABLE(CParserFunction);
	public:
		CParserFunction() = delete;
		CParserFunction(const CParserContext& ctx);
		~CParserFunction();

		[[nodiscard]] bloop::EStatus Parse();
		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
		[[nodiscard]] bloop::EStatus ParseDeclaration();
		[[nodiscard]] bloop::EStatus ParseParameters();
		[[nodiscard]] bloop::EStatus ParseParametersRecursively(std::vector<bloop::BloopString>& receiver);
		[[nodiscard]] bloop::EStatus ParseScope();

		const CParserContext& m_oCtx;

		bloop::CodePosition m_oDeclPos;
		bloop::BloopString m_sName;
		std::vector<bloop::BloopString> m_oParameters;
		std::unique_ptr<bloop::ast::BlockStatement> m_pBody;
	};
}