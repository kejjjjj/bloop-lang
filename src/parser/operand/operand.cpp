#include "parser/operand/operand.hpp"
#include "parser/parser.hpp"
#include "lexer/token.hpp"
#include "parser/exception.hpp"
#include "parser/expression/postfix/postfix.hpp"
#include "parser/expression/prefix/prefix.hpp"
#include "ast/ast.hpp"

#include <cassert>

using namespace bloop::parser;

CParserOperand::CParserOperand(const CParserContext& ctx) 
	: CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {
	assert(!IsEndOfBuffer());
}
CParserOperand::~CParserOperand() = default;

bloop::EStatus CParserOperand::Parse([[maybe_unused]]std::optional<PairMatcher>& eoe) {

	//todo: unary

	CParserPrefix prefix(m_oCtx);

	if (prefix.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;
	m_oPrefixes = prefix.GetPrefixes();

	const auto token = GetIteratorSafe();

	if (bloop::token::IsConstant(token->Type())) {
		m_pOperand = ParseConstant();
	} else if (token->IsOperator(EPunctuation::p_par_open)) {
		m_pOperand = ParseParentheses(eoe);
	} else if (token->IsOperator(EPunctuation::p_bracket_open)) {
		m_pOperand = ParseArray();
	} else if (token->IsOperator(EPunctuation::p_curlybracket_open)) {
		m_pOperand = ParseObject();
	} else if (token->Type() == ETokenType::tt_name) {
		m_pOperand = ParseIdentifier();
	} else {
		throw exception::ParserError(BLOOPTEXT("unsupported: ") + token->Source(), token->GetCodePosition());
	}

	//because it got overwritten
	m_pOperand->m_oDeclPos = token->GetCodePosition();

	if (IsEndOfBuffer())
		throw exception::ParserError(BLOOPTEXT("unexpected end of buffer"), GetIteratorSafe()->GetCodePosition());

	CParserPostfix postfix(m_oCtx);

	if (postfix.Parse() != bloop::EStatus::success)
		return bloop::EStatus::failure;

	m_oPostfixes = postfix.GetPostfixes();

	return bloop::EStatus::success;
}
static bloop::ast::BinaryExpression* SeekASTLeftBranch(bloop::ast::BinaryExpression* src) {

	auto end = src;

	while (end->left) {
		end = dynamic_cast<bloop::ast::BinaryExpression*>(end->left.get());
	}
	assert(end);
	return end;

}
std::unique_ptr<ASTExpression> CParserOperand::ToExpression() {

	std::unique_ptr<BinaryExpression> entry;

	//std::vector<std::unique_ptr<IPrefix>>* 

	for (auto& src : std::array<std::vector<std::unique_ptr<IPrefix>>*, 2>{&m_oPrefixes, &m_oPostfixes}) {
		if (auto&& pfs = UnaryToAST(*src)) {
			if (!entry)
				entry = std::move(pfs);
			else
				SeekASTLeftBranch(entry.get())->left = std::move(pfs);
		}
	}

	if (!entry) //no prefixes nor postfixes
		return GetOperand()->ToExpression();

	SeekASTLeftBranch(dynamic_cast<bloop::ast::BinaryExpression*>(entry.get()))->left = GetOperand()->ToExpression();
	return entry;
}
std::unique_ptr<BinaryExpression> CParserOperand::UnaryToAST(std::vector<std::unique_ptr<IPostfix>>& src) const noexcept {

	if (src.empty())
		return nullptr;

	std::unique_ptr<bloop::ast::BinaryExpression> root;;
	bloop::ast::BinaryExpression* position{};

	for (auto& pf : src) {

		if (!root) {
			root = pf->ToExpression();
			position = root.get();
			continue;
		}

		assert(position);
		position->left = pf->ToExpression();
		position = static_cast<bloop::ast::BinaryExpression*>(position->left.get());
	}

	return root;
}