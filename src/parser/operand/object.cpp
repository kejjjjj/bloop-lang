#include "parser/operand/object.hpp"
#include "ast/ast.hpp"
#include "parser/expression/expression.hpp"
#include "parser/exception.hpp"
#include "parser/parser.hpp"
#include "parser/operand/constant.hpp"

using namespace bloop::parser;

struct KVParser : public CParserSingle<bloop::CToken> {

	KVParser() = delete;
	KVParser(const CParserContext& ctx) : CParserSingle(ctx.m_iterPos, ctx.m_iterEnd), m_oCtx(ctx) {}

	[[nodiscard]] std::unique_ptr<CObjectOperand::KV> Parse(std::optional<PairMatcher> eoe) {

		if (IsEndOfBuffer() || EndOfExpression(eoe))
			return nullptr;

		const auto IsValidKey = [](const bloop::CToken* token) {
			return token->Type() == bloop::ETokenType::tt_name || bloop::token::IsConstant(token->Type());
		};

		if (IsEndOfBuffer() || !IsValidKey(GetIteratorSafe()))
			throw bloop::exception::ParserError(BLOOPTEXT("expected an identifier"), GetIteratorSafe()->GetCodePosition());

		//convert to string
		bloop::CToken identifier = *GetIteratorSafe();
		identifier.SetType(bloop::ETokenType::tt_string);

		Advance(1); // skip identifier

		if (IsEndOfBuffer() || !GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_colon))
			throw bloop::exception::ParserError(BLOOPTEXT("expected a \":\""), GetIteratorSafe()->GetCodePosition());

		Advance(1); //skip :

		CParserExpression expr(m_oCtx);

		if (expr.ParseInternal(eoe, nullptr, EEvaluationType::evaluate_singular) != bloop::EStatus::success)
			return nullptr;

		if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_comma))
			Advance(1); //skip comma

		return std::make_unique<CObjectOperand::KV>(CConstantOperand::FromToken(&identifier), expr.ToExpression());
	}

private:
	[[nodiscard]] bool EndOfExpression(const std::optional<PairMatcher>& eoe) const noexcept {
		assert(!IsEndOfBuffer());

		if (!eoe)
			return (*m_iterPos)->IsOperator(bloop::EPunctuation::p_semicolon);

		if (!(*m_iterPos)->IsOperator())
			return false;

		return eoe->IsClosing(dynamic_cast<const bloop::CPunctuationToken*>(*m_iterPos)->m_ePunctuation);
	}

	const CParserContext& m_oCtx;
};

std::unique_ptr<IOperand> CParserOperand::ParseObject() {

	Advance(1); // skip {

	if (!IsEndOfBuffer() && GetIteratorSafe()->IsOperator(bloop::EPunctuation::p_curlybracket_close)) {
		Advance(1); // skip }
		return std::make_unique<CObjectOperand>();
	}

	auto obj = std::make_unique<CObjectOperand>();
	auto parser = KVParser(m_oCtx);

	while (auto&& v = parser.Parse(PairMatcher(bloop::EPunctuation::p_curlybracket_open)))
		obj->m_oKeyValues.emplace_back(std::move(v));

	Advance(1); //skip }

	return obj;
}

CObjectOperand::CObjectOperand(std::vector<std::unique_ptr<KV>>&& kvs)
	: m_oKeyValues(std::forward<decltype(kvs)>(kvs)){}
CObjectOperand::~CObjectOperand() = default;

UniqueExpression CObjectOperand::ToExpression() {
	auto&& ptr =  std::make_unique<bloop::ast::ObjectExpression>(m_oDeclPos);
	ptr->m_oKeyValues = std::move(m_oKeyValues);
	return ptr;
}