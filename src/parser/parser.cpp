#include "ast/ast.hpp"
#include "ast/module.hpp"
#include "defs.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/declaration/declaration.hpp"
#include "parser/exception.hpp"
#include "parser/expression/expression.hpp"
#include "parser/function/function.hpp"
#include "parser/parser.hpp"
#include "parser/scope/scope.hpp"
#include "parser/control/while/while.hpp"
#include "parser/control/if/if.hpp"
#include "parser/control/for/for.hpp"
#include "parser/control/return/return.hpp"
#include "parser/control/control_flow/control_flow.hpp"
#include "parser/control/exception/try_catch.hpp"
#include "parser/control/exception/throw.hpp"

#include "utils/defs.hpp"
#include "utils/fmt.hpp"

#include <memory>

using namespace bloop::parser;

CLexParser::CLexParser(const bloop::lexer::CLexer& lexer, metadata::Metadata& metadata) 
	: m_refMetadata(metadata) {
	for (auto& t : lexer.GetTokens())
		m_oTokens.push_back(t.get());

	m_iterPos = m_oTokens.begin();
	m_iterEnd = m_oTokens.end();

	m_pInternal = std::make_unique<CLexParserInternal>(m_iterPos, m_iterEnd);
}
CLexParser::~CLexParser() = default;

std::unique_ptr<bloop::ast::Program> CLexParser::Parse(){
	assert(m_pInternal);
	return m_pInternal->Parse(m_refMetadata);
}

CLexParserInternal::CLexParserInternal(ParserIterator& start, ParserIterator& end)
	: CParser(start, end) {}
CLexParserInternal::~CLexParserInternal() = default;

std::unique_ptr<bloop::ast::Program> CLexParserInternal::Parse(metadata::Metadata& metadata)
{
	auto&& program = std::make_unique<ast::Program>(GetIteratorSafe()->GetCodePosition());

	auto ctx = CParserContext{
		.m_iterPos = m_iterPos,
		.m_iterEnd = m_iterEnd,
		.m_pCurrentBlock = program.get(),
		.m_refMetadata = metadata
	};

	// imports have to come before everything else, so it's obvious what a file depends on
	bool importsAllowed = true;

	while (!IsEndOfBuffer()) {

		const auto type = GetIteratorSafe()->Type();

		if (type == ETokenType::tt_import) {
			if (!importsAllowed)
				throw exception::ParserError(BLOOPTEXT("imports must come before everything else in the file"), GetIteratorSafe()->GetCodePosition());

			program->AddStatement(ParseImport());
		} else {
			importsAllowed = false;

			if (type == ETokenType::tt_export) {
				if (ParseExport(ctx) != bloop::EStatus::success)
					break;
			} else if (ParseToken(ctx) != bloop::EStatus::success) {
				break;
			}
		}

		if (IsEndOfBuffer())
			break;

		Advance(1);
	}

	return program;
}

std::unique_ptr<bloop::ast::Statement> CLexParserInternal::ParseImport() {

	// import name [, name]... from "path";
	auto statement = std::make_unique<ast::ImportStatement>(GetIteratorSafe()->GetCodePosition());
	Advance(1); // skip import

	const auto Expect = [this](bool ok, const bloop::BloopString& what) {
		if (IsEndOfBuffer())
			throw exception::ParserError(bloop::fmt::format(BLOOPTEXT("expected {} but reached the end of the file"), what), GetIteratorSafe()->GetCodePosition());
		if (!ok)
			throw exception::ParserError(bloop::fmt::format(BLOOPTEXT("expected {} instead of \"{}\""), what, GetIteratorSafe()->Source()), GetIteratorSafe()->GetCodePosition());
	};

	while (true) {
		Expect(!IsEndOfBuffer() && GetIteratorSafe()->Type() == ETokenType::tt_name, BLOOPTEXT("a name to import"));
		statement->m_oNames.push_back({ GetIteratorSafe()->Source(), GetIteratorSafe()->GetCodePosition() });
		Advance(1);

		Expect(!IsEndOfBuffer() && (GetIteratorSafe()->IsOperator(EPunctuation::p_comma) || GetIteratorSafe()->Type() == ETokenType::tt_name), 
			BLOOPTEXT("\",\" or \"from\""));

		if (GetIteratorSafe()->IsOperator(EPunctuation::p_comma)) {
			Advance(1);
			continue;
		}
		break;
	}

	// "from" isn't a keyword, so that it still works as a name elsewhere (std.string.from)
	Expect(GetIteratorSafe()->Source() == BLOOPTEXT("from"), BLOOPTEXT("\"from\""));
	Advance(1);

	Expect(!IsEndOfBuffer() && GetIteratorSafe()->Type() == ETokenType::tt_string, BLOOPTEXT("the path of the module as a string"));
	statement->m_sPath = GetIteratorSafe()->Source();
	if (statement->m_sPath.empty())
		throw exception::ParserError(BLOOPTEXT("the path of the module is empty"), GetIteratorSafe()->GetCodePosition());
	Advance(1);

	Expect(!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_semicolon), BLOOPTEXT("\";\""));
	return statement;
}

bloop::EStatus CLexParserInternal::ParseExport(const CParserContext& ctx) {

	// export let/const/fn ...
	const auto position = GetIteratorSafe()->GetCodePosition();
	Advance(1); // skip export

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("expected a declaration after \"export\""), position);

	const auto type = GetIteratorSafe()->Type();
	if (type != ETokenType::tt_let && type != ETokenType::tt_const && type != ETokenType::tt_fn)
		throw exception::ParserError(BLOOPTEXT("only \"let\", \"const\" and \"fn\" declarations can be exported"), GetIteratorSafe()->GetCodePosition());

	[[maybe_unused]] const auto count = ctx.m_pCurrentBlock->m_oStatements.size();
	const auto status = ParseToken(ctx);

	if (status == bloop::EStatus::success) {
		assert(ctx.m_pCurrentBlock->m_oStatements.size() == count + 1u);
		auto& declaration = ctx.m_pCurrentBlock->m_oStatements.back();

		// "fn" without a name is a lambda expression, not a declaration
		if (!declaration->GetDeclaredName())
			throw exception::ParserError(BLOOPTEXT("expected a declaration after \"export\""), position);

		declaration->m_bExported = true;
	}

	return status;
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
	return CreateParser<CParserExpressionStatement>(ctx);
}

bloop::EStatus bloop::parser::ParseToken(const CParserContext& ctx) {
	assert(ctx.GetIterator());
	switch (ctx.GetIterator()->Type()) {
	case ETokenType::tt_undefined:
	case ETokenType::tt_false:
	case ETokenType::tt_true:
	case ETokenType::tt_int:
	case ETokenType::tt_uint:
	case ETokenType::tt_double:
	case ETokenType::tt_string:
	case ETokenType::tt_template_head:
	case ETokenType::tt_name:
		return CreateParser<CParserExpressionStatement>(ctx);
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
	case ETokenType::tt_return:
		return CreateParser<CParserReturnStatement>(ctx);
	case ETokenType::tt_continue:
	case ETokenType::tt_break:
		return CreateParser<CParserControlStatement>(ctx);
	case ETokenType::tt_try:
		return CreateParser<CParserTryCatchStatement>(ctx);
	case ETokenType::tt_throw:
		return CreateParser<CParserThrowStatement>(ctx);
	case ETokenType::tt_import:
		throw exception::ParserError(BLOOPTEXT("imports are only allowed at the top of the file"), ctx.GetIterator()->GetCodePosition());
	case ETokenType::tt_export:
		throw exception::ParserError(BLOOPTEXT("only declarations in the global scope can be exported"), ctx.GetIterator()->GetCodePosition());
	default:
		throw exception::ParserError(BLOOPTEXT("unexpected token: ") + ctx.GetIterator()->Source(), ctx.GetIterator()->GetCodePosition());
	}
}