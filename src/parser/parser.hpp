#pragma once

#include "parser/defs.hpp"
#include "utils/defs.hpp"

#include <vector>
#include <memory>

namespace bloop::ast {
	struct BlockStatement;
	struct Program;
}
namespace bloop::lexer{
	class CLexer;
}
namespace bloop::parser {

	struct CParserContext {
		ParserIterator& m_iterPos;
		ParserIterator& m_iterEnd;
		mutable bloop::ast::BlockStatement* m_pCurrentBlock;

		auto& GetIterator() const noexcept { return *m_iterPos; }
	};

	class CLexParserInternal;

	class CLexParser final  {
	public:
		CLexParser(const bloop::lexer::CLexer& lexer);
		~CLexParser();

		[[nodiscard]] std::unique_ptr<bloop::ast::Program> Parse();

	private:
		std::vector<bloop::CToken*> m_oTokens;
		std::unique_ptr<CLexParserInternal> m_pInternal;
		ParserIterator m_iterPos, m_iterEnd;
	};

	class CLexParserInternal : public CParser {
	public:
		CLexParserInternal(ParserIterator& start, ParserIterator& end);
		~CLexParserInternal();

		[[nodiscard]] std::unique_ptr<bloop::ast::Program> Parse();

	private:
	};

	//other parsers might utilize this
	[[nodiscard]] bloop::EStatus ParseToken(const CParserContext& ctx);

}