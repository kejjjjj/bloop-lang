#include "parser/operand/constant.hpp"
#include "lexer/token.hpp"
#include "ast/ast.hpp"

#include <unordered_map>
#include <cassert>
#include <charconv>
using namespace bloop::parser;

std::unique_ptr<IOperand> CParserOperand::ParseConstant() {
	auto&& v = std::make_unique<CConstantOperand>(*m_iterPos);
	Advance(1);
	return v;
}

std::unique_ptr<ASTExpression> CConstantOperand::ToExpression(){
	auto&& ptr = std::make_unique<bloop::ast::LiteralExpression>();
	ptr->m_eDataType = GetType();
	ptr->m_pConstant = ToData();
	return ptr;
}

bloop::EValueType CConstantOperand::GetType() const noexcept {

	using namespace bloop;

	const std::unordered_map<ETokenType, EValueType> map = {
		{ ETokenType::tt_undefined, EValueType::t_undefined },
		{ ETokenType::tt_false, EValueType::t_boolean},
		{ ETokenType::tt_true, EValueType::t_boolean},
		{ ETokenType::tt_int, EValueType::t_int},
		{ ETokenType::tt_uint, EValueType::t_uint},
		{ ETokenType::tt_double, EValueType::t_double},
		{ ETokenType::tt_string, EValueType::t_string},
	};

	assert(map.contains(m_pToken->Type()));
	return map.at(m_pToken->Type());

}
std::size_t CConstantOperand::GetSize() const noexcept {
	const auto type = GetType();

	switch (type) {
	case EValueType::t_undefined:
		return std::size_t(0);
	case EValueType::t_boolean:
		return sizeof(bool);
	case EValueType::t_int:
		return sizeof(BloopInt);
	case EValueType::t_uint:
		return sizeof(BloopUInt);
	case EValueType::t_double:
		return sizeof(BloopDouble);
	case EValueType::t_string:
		return m_pToken->Source().size();
	case EValueType::t_callable:
		return sizeof(void*);
	default:
		assert(false);
		return 0u;
	}
}
bloop::BloopString CConstantOperand::ToData() const noexcept {

	if (GetType() == EValueType::t_string)
		return m_pToken->Source();

	BloopString result;
	BloopDouble dvalue;
	BloopString string = m_pToken->Source();

	switch (GetType()) {
	case EValueType::t_undefined:
		return BLOOPTEXT("");
	case EValueType::t_boolean:
		return BloopString(1, m_pToken->Type() == ETokenType::tt_true ? BloopChar('\x01') : BloopChar('\x00'));
	case EValueType::t_int:
		result = BloopString(sizeof(BloopInt), 0);
		std::from_chars(string.c_str(), string.c_str() + string.size(), reinterpret_cast<BloopInt&>(*result.data()));
		break;
	case EValueType::t_uint:
		result = BloopString(sizeof(BloopUInt), 0);
		std::from_chars(string.c_str(), string.c_str() + string.size(), reinterpret_cast<BloopUInt&>(*result.data()));
		break;
	case EValueType::t_double:
		result = BloopString(sizeof(BloopDouble), 0);
		dvalue = std::strtod(string.c_str(), nullptr);
		std::memcpy(result.data(), &dvalue, sizeof(BloopDouble));
		break;
	case EValueType::t_string:
	default:
		assert(false);
	}
	return result;

}