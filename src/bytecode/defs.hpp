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
		{ EPunctuation::p_less_equal, EOpCode::LESS_EQUAL },
		{ EPunctuation::p_equality, EOpCode::EQ },
		{ EPunctuation::p_strict_equality, EOpCode::S_EQ },
		{ EPunctuation::p_unequality, EOpCode::NE },
		{ EPunctuation::p_strict_unequality, EOpCode::S_NE },
		{ EPunctuation::p_comma, EOpCode::SEQUENCE },
	};

	static std::unordered_map<EOpCode, bloop::BloopString> stringConversionTable = {
		#define BLOOP_OP(name) { EOpCode::name, #name },
		#include "opcode.def"
		#undef BLOOP_OP
	};

}
