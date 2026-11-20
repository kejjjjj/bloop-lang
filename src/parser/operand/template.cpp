#include "parser/operand/template.hpp"
#include "ast/ast.hpp"
#include "lexer/token.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"

using namespace bloop::parser;

// the lexer gives: head (, tokens, ) middle (, tokens, ) ... tail.
// Each substitution is parsed like an expression in parentheses
std::unique_ptr<IOperand> CParserOperand::ParseTemplate() {

	std::vector<UniqueExpression> parts;

	const auto addText = [&parts](const bloop::CToken* token) {
		if (token->Source().empty())
			return; // nothing to add
		auto literal = std::make_unique<bloop::ast::LiteralExpression>(token->GetCodePosition());
		literal->m_oConstant = { token->Source(), bloop::EValueType::t_string };
		parts.emplace_back(std::move(literal));
	};

	assert(GetIteratorSafe()->Type() == ETokenType::tt_template_head);
	addText(GetIteratorSafe());
	Advance(1);

	while (true) {
		if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(EPunctuation::p_par_open))
			throw exception::ParserError(BLOOPTEXT("expected a substitution in the template string"), GetIteratorSafe()->GetCodePosition());
		Advance(1); // (

		if (IsEndOfBuffer() || GetIteratorSafe()->IsOperator(EPunctuation::p_par_close))
			throw exception::ParserError(BLOOPTEXT("a ${} in a template string needs an expression"), GetIteratorSafe()->GetCodePosition());

		CParserExpression expr(m_oCtx);
		if (expr.Parse(PairMatcher(EPunctuation::p_par_open)) != bloop::EStatus::success) // (also skips the ))
			throw exception::ParserError(BLOOPTEXT("failed to parse the expression"), GetIteratorSafe()->GetCodePosition());
		parts.emplace_back(expr.ToExpression());

		if (IsEndOfBuffer())
			throw exception::ParserError(BLOOPTEXT("the template string doesn't end"), GetIteratorSafe()->GetCodePosition());

		const auto* text = GetIteratorSafe();
		if (text->Type() != ETokenType::tt_template_middle && text->Type() != ETokenType::tt_template_tail)
			throw exception::ParserError(BLOOPTEXT("expected the rest of the template string"), text->GetCodePosition());

		addText(text);
		Advance(1);

		if (text->Type() == ETokenType::tt_template_tail)
			break;
	}

	return std::make_unique<CTemplateOperand>(std::move(parts));
}

CTemplateOperand::CTemplateOperand(std::vector<UniqueExpression>&& parts)
	: m_oParts(std::move(parts)) {}
CTemplateOperand::~CTemplateOperand() = default;

UniqueExpression CTemplateOperand::ToExpression() {
	return std::make_unique<bloop::ast::TemplateExpression>(std::move(m_oParts), m_oDeclPos);
}
