#include "parser/function/function.hpp"
#include "parser/parser.hpp"
#include "parser/exception.hpp"
#include "parser/scope/scope.hpp"
#include "lexer/token.hpp"
#include "ast/function.hpp"

#include <cassert>

using namespace bloop::parser;

CParserFunction::CParserFunction(const CParserContext& ctx)
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserFunction::~CParserFunction() = default;

bloop::EStatus CParserFunction::Parse() {
	
	if (ParseDeclaration() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	if (ParseScope() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	return bloop::EStatus::success;
}
bloop::EStatus CParserFunction::ParseDeclaration() {

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != ETokenType::tt_fn)
		throw exception::ParserError(BLOOPTEXT("expected \"fn\""), GetIteratorSafe()->GetCodePosition());

	m_oDeclPos = GetIteratorSafe()->GetCodePosition();

	Advance(1); //skip fn

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != ETokenType::tt_name)
		throw exception::ParserError(BLOOPTEXT("expected an identifier"), GetIteratorSafe()->GetCodePosition());

	m_sName = GetIteratorSafe()->Source();

	Advance(1); //skip identifier

	return ParseParameters();
}
bloop::EStatus CParserFunction::ParseParameters() {

	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(EPunctuation::p_par_open))
		throw exception::ParserError(BLOOPTEXT("expected \"(\""), GetIteratorSafe()->GetCodePosition());

	Advance(1); //skip (

	//no parameters?
	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(EPunctuation::p_par_close)) {
		Advance(1);// skip )
		return bloop::EStatus::success;
	}

	if (ParseParametersRecursively(m_oParameters) != bloop::EStatus::success)
		return bloop::EStatus::failure;

	Advance(1); // skip )
	return bloop::EStatus::success;
}

bloop::EStatus CParserFunction::ParseParametersRecursively(std::vector<bloop::BloopString>& receiver) {

	if (IsEndOfBuffer() || GetIteratorSafe()->Type() != ETokenType::tt_name)
		throw exception::ParserError(BLOOPTEXT("expected an identifier"), GetIteratorSafe()->GetCodePosition());

	receiver.push_back(GetIteratorSafe()->Source());
	Advance(1); // skip identifier

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_comma)) {
		Advance(1); //skip ,
		return ParseParametersRecursively(receiver);
	}

	if (GetIteratorSafe()->IsOperator(EPunctuation::p_par_close))
		return bloop::EStatus::success;

	throw exception::ParserError(BLOOPTEXT("expected \",\" or \")\""));
}

bloop::EStatus CParserFunction::ParseScope() {
	if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(EPunctuation::p_curlybracket_open))
		throw exception::ParserError(BLOOPTEXT("expected \"{\""), GetIteratorSafe()->GetCodePosition());

	CParserScope parser(m_oCtx);
	m_pBody = parser.Parse();

	//Advance(-1); // so that the parser doesn't skip the next token

	return m_pBody ? bloop::EStatus::success : bloop::EStatus::failure;
}

UniqueStatement CParserFunction::ToStatement() {
	return std::make_unique<bloop::ast::FunctionDeclarationStatement>(m_sName, std::move(m_oParameters), std::move(m_pBody), m_oDeclPos);
}