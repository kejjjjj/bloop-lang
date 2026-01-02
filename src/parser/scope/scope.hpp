#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"

#include <vector>
#include <memory>

namespace bloop::ast {
	struct UnnamedScopeStatement;
}

namespace bloop::parser {
	struct CParserContext;


	class CParserScope final : CParserSingle<bloop::CToken> {
		BLOOP_NONCOPYABLE(CParserScope);


		struct FirstData {
			bloop::CodePosition cp;
			bool isCurlyBracket{};
		};

	public:
		CParserScope() = delete;
		CParserScope(const CParserContext& ctx);
		~CParserScope();

		[[nodiscard]] std::unique_ptr<bloop::ast::UnnamedScopeStatement> Parse(bool allowSingleStatement=false);
		
		//append to current scope
		void ParseNoScope(bool allowSingleStatement = false);

	private:
		[[nodiscard]] FirstData ParseFirstPart(bool allowSingleStatement);
		void ParseSecondPart(bool allowSingleStatement, bool isCurlyBracket);

		const CParserContext& m_oCtx;

	};

}