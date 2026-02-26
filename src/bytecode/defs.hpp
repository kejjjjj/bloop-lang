#pragma once

#include "lexer/punctuation.hpp"
#include "bc_common.hpp"

#include <unordered_map>
#include <vector>

namespace bloop::bytecode
{

	enum class EOpCode : unsigned char {
		#define BLOOP_OP(name) name,
		#include "opcode.def"
		#undef BLOOP_OP
	};

	static std::unordered_map<EPunctuation, EOpCode> conversionTable = {
		{ EPunctuation::p_add, EOpCode::ADD },
		{ EPunctuation::p_sub, EOpCode::SUB },
		{ EPunctuation::p_multiplication, EOpCode::MUL },
		{ EPunctuation::p_division, EOpCode::DIV },
		{ EPunctuation::p_modulo, EOpCode::MOD },
		{ EPunctuation::p_less_than, EOpCode::LESS },
		{ EPunctuation::p_less_equal, EOpCode::LESS_EQUAL },
		{ EPunctuation::p_greater_than, EOpCode::GREATER },
		{ EPunctuation::p_greater_equal, EOpCode::GREATER_EQUAL },
		{ EPunctuation::p_logical_and, EOpCode::LOGICAL_AND },
		{ EPunctuation::p_logical_or, EOpCode::LOGICAL_OR },
		{ EPunctuation::p_equality, EOpCode::EQ },
		{ EPunctuation::p_strict_equality, EOpCode::S_EQ },
		{ EPunctuation::p_unequality, EOpCode::NE },
		{ EPunctuation::p_strict_unequality, EOpCode::S_NE },
		{ EPunctuation::p_left_shift, EOpCode::LEFT_SHIFT },
		{ EPunctuation::p_right_shift, EOpCode::RIGHT_SHIFT },
		{ EPunctuation::p_bitwise_and, EOpCode::BITWISE_AND },
		{ EPunctuation::p_bitwise_or, EOpCode::BITWISE_OR },
		{ EPunctuation::p_bitwise_xor, EOpCode::BITWISE_XOR },
		{ EPunctuation::p_comma, EOpCode::SEQUENCE },
	};

	static std::unordered_map<EOpCode, bloop::BloopString> stringConversionTable = {
		#define BLOOP_OP(name) { EOpCode::name, #name },
		#include "opcode.def"
		#undef BLOOP_OP
	};

}
