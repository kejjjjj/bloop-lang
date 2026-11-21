#pragma once
#include <cassert>
#include <algorithm>
#include <ranges>
#include <array>
#include <utility>

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

		#define BLOOP_X(name) tt_##name,
		#include "token_keywords.def"
		#undef BLOOP_X

		// `text ${a} text ${b} text`: the text before the first substitution, between two, and after the last.
		// The lexer puts each substitution's tokens between ( and ) tokens. (Without substitutions it's a tt_string)
		tt_template_head,
		tt_template_middle,
		tt_template_tail,
	};
	namespace token {
		[[nodiscard]] constexpr auto IsConstant(ETokenType t) {
			constexpr std::array<ETokenType, 7> constants = {
				ETokenType::tt_undefined, ETokenType::tt_false, ETokenType::tt_true,
				ETokenType::tt_int, ETokenType::tt_uint, ETokenType::tt_double, ETokenType::tt_string
			};
			return std::ranges::any_of(constants, [t](ETokenType _t) { return _t == t; });
		}

		inline std::pair<ETokenType, bloop::BloopString> tokenNames[] = {
			{ ETokenType::tt_error,    BLOOPTEXT("error") },
			{ ETokenType::tt_int,      BLOOPTEXT("int") },
			{ ETokenType::tt_uint,     BLOOPTEXT("uint") },
			{ ETokenType::tt_double,   BLOOPTEXT("double") },
			{ ETokenType::tt_string,   BLOOPTEXT("string") },
			{ ETokenType::tt_name,     BLOOPTEXT("name") },
			{ ETokenType::tt_operator, BLOOPTEXT("operator")},

			#define BLOOP_X(name) { ETokenType::tt_##name, BLOOPTEXT(#name) },
			#include "token_keywords.def"
			#undef BLOOP_X

			{ ETokenType::tt_template_head,   BLOOPTEXT("template string") },
			{ ETokenType::tt_template_middle, BLOOPTEXT("template string") },
			{ ETokenType::tt_template_tail,   BLOOPTEXT("template string") },
		};

		inline bloop::BloopString TokenName(ETokenType t) {
			for (const auto& [k, v] : tokenNames)
				if (k == t) return v;
			return "unknown";
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
		constexpr void SetType(ETokenType t) noexcept { m_eTokenType = t; }
		[[nodiscard]] virtual bool IsOperator() const noexcept { return false; }
		[[nodiscard]] virtual bool IsOperator([[maybe_unused]] EPunctuation p) const noexcept { return false; }
		[[nodiscard]] constexpr auto& Source() const noexcept { return m_sSource; }
		[[nodiscard]] constexpr auto& GetCodePosition() const noexcept { return m_oSourcePosition; }
		[[nodiscard]] virtual constexpr CPunctuationToken* GetPunctuation() noexcept { return nullptr; }
		[[nodiscard]] virtual constexpr const CPunctuationToken* GetPunctuation() const noexcept { return nullptr; }
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
		[[nodiscard]] constexpr const CPunctuationToken* GetPunctuation() const noexcept override { return this; }

		EPunctuation m_ePunctuation{};
		EOperatorPriority m_ePriority{};
	};
}