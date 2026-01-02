#include "ast/ast.hpp"
#include "defs.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/declaration/declaration.hpp"
#include "parser/exception.hpp"
#include "parser/expression/expression.hpp"
#include "parser/function/function.hpp"
#include "parser/parser.hpp"
#include "parser/scope/scope.hpp"
#include "parser/statements/while/while.hpp"
#include "parser/statements/if/if.hpp"
#include "parser/statements/for/for.hpp"

#include "parser/statements/return/return.hpp"

#include "utils/defs.hpp"

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

std::unique_ptr<bloop::ast::Program> CLexParser::Parse(){
	assert(m_pInternal);
	return m_pInternal->Parse();
}

CLexParserInternal::CLexParserInternal(ParserIterator& start, ParserIterator& end)
	: CParser(start, end) {}
CLexParserInternal::~CLexParserInternal() = default;

std::unique_ptr<bloop::ast::Program> CLexParserInternal::Parse()
{
	auto&& program = std::make_unique<ast::Program>(GetIteratorSafe()->GetCodePosition());

	auto ctx = CParserContext{
		.m_iterPos = m_iterPos,
		.m_iterEnd = m_iterEnd,
		.m_pCurrentBlock = program.get()
	};

	while (!IsEndOfBuffer()) {

		if (ParseToken(ctx) != bloop::EStatus::success)
			break;

		if (IsEndOfBuffer())
			break;

		Advance(1);
	}

	return program;
}

template<typename Parser>
bloop::EStatus CreateParser(const bloop::parser::CParserContext& ctx) {

	Parser parser(ctx);
	if (parser.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	ctx.m_pCurrentBlock->AddStatement(parser.ToStatement());
	return bloop::EStatus::success;
}

static auto ParseOperator(const CParserContext& ctx) {

	//new scope
	if (ctx.GetIterator()->IsOperator(bloop::EPunctuation::p_curlybracket_open)) {
		bloop::parser::CParserScope sc(ctx);
		ctx.m_pCurrentBlock->AddStatement(sc.Parse());
		return bloop::EStatus::success;
	}

	// normal expression otherwise
	return CreateParser<CParserExpression>(ctx);
}

bloop::EStatus bloop::parser::ParseToken(const CParserContext& ctx) {

	switch (ctx.GetIterator()->Type()) {
	case ETokenType::tt_undefined:
	case ETokenType::tt_false:
	case ETokenType::tt_true:
	case ETokenType::tt_int:
	case ETokenType::tt_uint:
	case ETokenType::tt_double:
	case ETokenType::tt_string:
	case ETokenType::tt_name:
		return CreateParser<CParserExpression>(ctx);
	case ETokenType::tt_operator:
		return ParseOperator(ctx);
	case ETokenType::tt_fn:
		return CreateParser<CParserFunction>(ctx);
	case ETokenType::tt_let:
	case ETokenType::tt_const:
		return CreateParser<CParserDeclaration>(ctx);
	case ETokenType::tt_while:
		return CreateParser<CParserWhileStatement>(ctx);
	case ETokenType::tt_if:
		return CreateParser<CParserIfStatement>(ctx);
	case ETokenType::tt_for:
		return CreateParser<CParserForStatement>(ctx);
	case ETokenType::tt_else:
		throw exception::ParserError(BLOOPTEXT("unexpected else statement"), ctx.GetIterator()->GetCodePosition());
	case ETokenType::tt_return:
		return CreateParser<CParserReturnStatement>(ctx);
	default:
		throw exception::ParserError(BLOOPTEXT("unexpected token: ") + ctx.GetIterator()->Source(), ctx.GetIterator()->GetCodePosition());
	}
}