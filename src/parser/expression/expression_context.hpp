#pragma once

#include <cassert>
#include <unordered_map>
#include "lexer/punctuation.hpp"

namespace bloop::parser {

	class PairMatcher final {
	public:
		PairMatcher(EPunctuation p) {
			const std::unordered_map<EPunctuation, EPunctuation> map = {
				{ EPunctuation::p_par_open, EPunctuation::p_par_close },
				{ EPunctuation::p_bracket_open, EPunctuation::p_bracket_close },
				{ EPunctuation::p_curlybracket_open, EPunctuation::p_curlybracket_close },
				{ EPunctuation::p_comma, EPunctuation::p_comma },
				{ EPunctuation::p_colon, EPunctuation::p_colon }
			};

			assert(map.contains(p));
			m_eClosingPunctuation = map.at(p);
		}
		[[nodiscard]] bool IsClosing(EPunctuation p) const noexcept {
			return p == m_eClosingPunctuation;
		}
	private:
		EPunctuation m_eClosingPunctuation{ EPunctuation::p_error };
	};

	enum class EEvaluationType : signed char {
		evaluate_everything,
		evaluate_singular
	};
}