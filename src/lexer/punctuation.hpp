#pragma once

#include "utils/defs.hpp"

#include <array>

namespace bloop {

	enum class EOperatorPriority : char
	{
		op_failure,
		op_assignment,		//	= += -= *= /= %= >>= <<= &= ^= |= <=>
		op_conditional,		//	?
		op_conditional2,    //  :  
		op_logical_or,		//	||
		op_logical_and,		//	&&
		op_bitwise_or,		//  | 
		op_bitwise_xor,		//	^
		op_bitwise_and,		//  & 
		op_equality,		//	< <= > >=
		op_relational,		//	== != === !==
		op_shift,			//  <<>>
		op_additive,		//	+ -
		op_multiplicative,	//	* / %
		op_unary,			//  + - ! ~ ++ - - (type)* & sizeof	
		op_postfix,			//  () [] -> . ++ --	

	};

	enum class EPunctuation : char
	{
		p_error,
		p_add,
		p_sub,
		p_multiplication,
		p_division,
		p_modulo,

		p_less_than,
		p_less_equal,
		p_greater_than,
		p_greater_equal,


		p_equality,
		p_unequality,
		p_strict_equality,
		p_strict_unequality,

		p_logical_and,
		p_logical_or,

		p_left_shift,
		p_right_shift,

		p_bitwise_or,
		p_bitwise_xor,
		p_bitwise_and,

		p_assign,
		p_assignment_addition,
		p_assignment_subtraction,
		p_assignment_multiplication,
		p_assignment_division,
		p_assignment_modulo,

		p_assignment_left_shift,
		p_assignment_right_shift,

		p_assignment_bitwise_or,
		p_assignment_bitwise_xor,
		p_assignment_bitwise_and,

		p_swap,

		p_increment,
		p_decrement,

		p_par_open,
		p_par_close,

		p_bracket_open,
		p_bracket_close,

		p_curlybracket_open,
		p_curlybracket_close,

		p_comma,
		p_period,
		p_semicolon,
		p_colon,
		p_question_mark,
		p_exclamation,
		p_tilde,
		p_spread,
		p_num_punctuations
	};

	struct CPunctuation final
	{
		BloopStringView m_sIdentifier;
		EPunctuation m_ePunctuation{};
		EOperatorPriority m_ePriority{};
	};

	constexpr std::array<CPunctuation, static_cast<std::size_t>(EPunctuation::p_num_punctuations) - 1u> punctuations{
		CPunctuation{BLOOPTEXT("==="), EPunctuation::p_strict_equality, EOperatorPriority::op_relational},
		CPunctuation{BLOOPTEXT("!=="), EPunctuation::p_strict_unequality, EOperatorPriority::op_relational},

		CPunctuation{BLOOPTEXT("<<="), EPunctuation::p_assignment_left_shift, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT(">>="), EPunctuation::p_assignment_right_shift, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("<=>"), EPunctuation::p_swap, EOperatorPriority::op_assignment},

		CPunctuation{BLOOPTEXT("..."), EPunctuation::p_spread, EOperatorPriority::op_unary},

		CPunctuation{BLOOPTEXT("+="), EPunctuation::p_assignment_addition, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("-="), EPunctuation::p_assignment_subtraction, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("*="), EPunctuation::p_assignment_multiplication, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("/="), EPunctuation::p_assignment_division, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("%="), EPunctuation::p_assignment_modulo, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("|="), EPunctuation::p_assignment_bitwise_or, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("^="), EPunctuation::p_assignment_bitwise_xor, EOperatorPriority::op_assignment},
		CPunctuation{BLOOPTEXT("&="), EPunctuation::p_assignment_bitwise_and, EOperatorPriority::op_assignment},

		CPunctuation{BLOOPTEXT("=="), EPunctuation::p_equality, EOperatorPriority::op_relational},
		CPunctuation{BLOOPTEXT("!="), EPunctuation::p_unequality, EOperatorPriority::op_relational},

		CPunctuation{BLOOPTEXT("<="), EPunctuation::p_less_equal, EOperatorPriority::op_equality},
		CPunctuation{BLOOPTEXT(">="), EPunctuation::p_greater_equal, EOperatorPriority::op_equality},

		CPunctuation{BLOOPTEXT("&&"), EPunctuation::p_logical_and, EOperatorPriority::op_logical_and},
		CPunctuation{BLOOPTEXT("||"), EPunctuation::p_logical_or, EOperatorPriority::op_logical_or},

		CPunctuation{BLOOPTEXT("<<"), EPunctuation::p_left_shift, EOperatorPriority::op_shift},
		CPunctuation{BLOOPTEXT(">>"), EPunctuation::p_right_shift, EOperatorPriority::op_shift},

		CPunctuation{BLOOPTEXT("++"), EPunctuation::p_increment, EOperatorPriority::op_unary},
		CPunctuation{BLOOPTEXT("--"), EPunctuation::p_decrement, EOperatorPriority::op_unary},
		CPunctuation{BLOOPTEXT("~"), EPunctuation::p_tilde, EOperatorPriority::op_unary},

		CPunctuation{BLOOPTEXT("+"), EPunctuation::p_add, EOperatorPriority::op_additive},
		CPunctuation{BLOOPTEXT("-"), EPunctuation::p_sub, EOperatorPriority::op_additive},

		CPunctuation{BLOOPTEXT("<"), EPunctuation::p_less_than, EOperatorPriority::op_equality},
		CPunctuation{BLOOPTEXT(">"), EPunctuation::p_greater_than, EOperatorPriority::op_equality},

		CPunctuation{BLOOPTEXT("%"), EPunctuation::p_modulo, EOperatorPriority::op_multiplicative},
		CPunctuation{BLOOPTEXT("*"), EPunctuation::p_multiplication, EOperatorPriority::op_multiplicative},
		CPunctuation{BLOOPTEXT("/"), EPunctuation::p_division, EOperatorPriority::op_multiplicative},

		CPunctuation{BLOOPTEXT("="), EPunctuation::p_assign, EOperatorPriority::op_assignment},

		CPunctuation{BLOOPTEXT("|"), EPunctuation::p_bitwise_or, EOperatorPriority::op_bitwise_or},
		CPunctuation{BLOOPTEXT("^"), EPunctuation::p_bitwise_xor, EOperatorPriority::op_bitwise_xor},
		CPunctuation{BLOOPTEXT("&"), EPunctuation::p_bitwise_and, EOperatorPriority::op_bitwise_and},

		CPunctuation{BLOOPTEXT("("), EPunctuation::p_par_open, EOperatorPriority::op_postfix},
		CPunctuation{BLOOPTEXT(")"), EPunctuation::p_par_close, EOperatorPriority::op_failure},

		CPunctuation{BLOOPTEXT("["), EPunctuation::p_bracket_open, EOperatorPriority::op_postfix},
		CPunctuation{BLOOPTEXT("]"), EPunctuation::p_bracket_close, EOperatorPriority::op_failure},

		CPunctuation{BLOOPTEXT("{"), EPunctuation::p_curlybracket_open, EOperatorPriority::op_failure},
		CPunctuation{BLOOPTEXT("}"), EPunctuation::p_curlybracket_close, EOperatorPriority::op_failure},

		CPunctuation{BLOOPTEXT(","), EPunctuation::p_comma},
		CPunctuation{BLOOPTEXT("."), EPunctuation::p_period, EOperatorPriority::op_postfix},
		CPunctuation{BLOOPTEXT(";"), EPunctuation::p_semicolon },
		CPunctuation{BLOOPTEXT(":"), EPunctuation::p_colon, EOperatorPriority::op_conditional2},
		CPunctuation{BLOOPTEXT("?"), EPunctuation::p_question_mark, EOperatorPriority::op_conditional},
		CPunctuation{BLOOPTEXT("!"), EPunctuation::p_exclamation, EOperatorPriority::op_unary}
	};

}
