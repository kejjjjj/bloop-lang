#pragma once
#include <cassert>
#include <algorithm>
#include <ranges>
#include <array>

#include "utils/defs.hpp"
#include "lexer/punctuation.hpp"

namespace bloop {
	namespace lexer {
		class CLexer;
	}
	enum class ETokenType : unsigned char {
		tt_error,
		tt_int,
		tt_uint,
		tt_double,
		tt_string,
		tt_name,
		tt_operator,

		#define X(name) tt_##name,
		#include "token_keywords.def"
		#undef X

	};
	namespace token {
		[[nodiscard]] constexpr auto IsConstant(ETokenType t) {
			constexpr std::array<ETokenType, 7> constants = {
				ETokenType::tt_undefined, ETokenType::tt_false, ETokenType::tt_true,
				ETokenType::tt_int, ETokenType::tt_uint, ETokenType::tt_double, ETokenType::tt_string
			};
			return std::ranges::any_of(constants, [t](ETokenType _t) { return _t == t; });
		}
	}
	class CPunctuationToken;
	class CToken
	{
		friend class lexer::CLexer;
	public:
		constexpr CToken() = default;
		constexpr CToken(BloopStringView token, ETokenType tt) : m_eTokenType(tt), m_sSource(token) {
			assert(token.data() && token.size());
			assert(m_eTokenType != ETokenType::tt_error);
		}
		constexpr virtual ~CToken() = default;

		[[nodiscard]] constexpr auto Type() const noexcept { return m_eTokenType; }
		[[nodiscard]] virtual bool IsOperator() const noexcept { return false; }
		[[nodiscard]] virtual bool IsOperator([[maybe_unused]] EPunctuation p) const noexcept { return false; }
		[[nodiscard]] constexpr auto& Source() const noexcept { return m_sSource; }
		[[nodiscard]] constexpr auto& GetCodePosition() const noexcept { return m_oSourcePosition; }
		[[nodiscard]] virtual constexpr CPunctuationToken* GetPunctuation() noexcept { return nullptr; }
	private:
		ETokenType m_eTokenType{ ETokenType::tt_error };
		CodePosition m_oSourcePosition{ 1, 1 };
	protected:
		BloopString m_sSource;
	};

	class CPunctuationToken final : public CToken
	{
	public:
		constexpr CPunctuationToken(const CPunctuation& p)
			: CToken(p.m_sIdentifier, ETokenType::tt_operator), m_ePunctuation(p.m_ePunctuation), m_ePriority(p.m_ePriority) {
		}
		constexpr ~CPunctuationToken() = default;

		[[nodiscard]] bool IsOperator() const noexcept override { return true; }
		[[nodiscard]] bool IsOperator(EPunctuation p) const noexcept override { return m_ePunctuation == p; }

		[[nodiscard]] constexpr CPunctuationToken* GetPunctuation() noexcept override { return this; }

		EPunctuation m_ePunctuation{};
		EOperatorPriority m_ePriority{};
	};
}