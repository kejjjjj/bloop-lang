#include "ast/ast.hpp"
#include "defs.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"
#include "utils/defs.hpp"
#include "parser/expression/expression.hpp"

#include <memory>

using namespace bloop::parser;

CLexParser::CLexParser(const bloop::lexer::CLexer& lexer) {
	for (auto& t : lexer.GetTokens())
		m_oTokens.push_back(t.get());

	m_iterPos = m_oTokens.begin();
	m_iterEnd = m_oTokens.end();

	m_pInternal = std::make_unique<CLexParserInternal>(m_iterPos, m_iterEnd);
}
CLexParser::~CLexParser() = default;

bloop::EStatus CLexParser::Parse(){
	assert(m_pInternal);
	return m_pInternal->Parse();
}

CLexParserInternal::CLexParserInternal(ParserIterator& start, ParserIterator& end)
	: CParser(start, end) {}
CLexParserInternal::~CLexParserInternal() = default;

bloop::EStatus CLexParserInternal::Parse()
{
	auto program = ast::Program();

	auto ctx = CParserContext{
		.m_iterPos = m_iterPos,
		.m_iterEnd = m_iterEnd,
		.m_pCurrentBlock = &program
	};

	while (!IsEndOfBuffer()) {

		if (ParseToken(ctx) != bloop::EStatus::success)
			break;

		if (IsEndOfBuffer())
			break;

		Advance(1);
	}

	return bloop::EStatus::success;
}

template<typename Parser>
bloop::EStatus CreateParser(const bloop::parser::CParserContext& ctx) {

	Parser parser(ctx);
	if (parser.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	ctx.m_pCurrentBlock->AddStatement(parser.ToAST());
	return bloop::EStatus::success;
}

bloop::EStatus CLexParserInternal::ParseToken(const CParserContext& ctx) {

	if (IsEndOfBuffer())
		return bloop::EStatus::failure;

	switch (ctx.GetIterator()->Type()) {
	case ETokenType::tt_undefined:
	case ETokenType::tt_false:
	case ETokenType::tt_true:
	case ETokenType::tt_int:
	case ETokenType::tt_uint:
	case ETokenType::tt_double:
	case ETokenType::tt_string:
	case ETokenType::tt_name:
	case ETokenType::tt_operator:
		CreateParser<CParserExpression>(ctx);
		break;
	default:
		throw exception::ParserError(BLOOPTEXT("unexpected token: ") + ctx.GetIterator()->Source(), ctx.GetIterator()->GetCodePosition());
	}

	return bloop::EStatus::failure;

}