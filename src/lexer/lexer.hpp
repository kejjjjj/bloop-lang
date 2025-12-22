#pragma once

#include "lexer/token.hpp"

#include <memory>
#include <vector>

namespace bloop::lexer {

	class CLexer final {
	public:
		CLexer(bloop::BloopStringView buffer);
		~CLexer();

		[[nodiscard]] void Parse();
		[[nodiscard]] auto& GetTokens() const { return m_oTokens; }

	private:
		[[nodiscard]] constexpr bool EndOfBuffer() const noexcept { return m_oScriptPos == m_oScriptEnd; }
		[[nodiscard]] bool IsToken(bloop::BloopStringView t) noexcept;

		[[nodiscard]] std::unique_ptr<bloop::CToken> ReadToken();

		[[nodiscard]] bloop::EStatus ReadWhiteSpace() noexcept;
		[[nodiscard]] bloop::EStatus ReadSingleLineComment() noexcept;
		[[nodiscard]] bloop::EStatus ReadMultiLineComment();

		[[nodiscard]] bloop::EStatus ReadNumber(bloop::CToken& token);
		[[nodiscard]] bloop::EStatus ReadInteger(bloop::CToken& token);
		[[nodiscard]] bloop::EStatus ReadHex(bloop::CToken& token);

		[[nodiscard]] bloop::EStatus ReadString(bloop::CToken& token, bloop::BloopChar quote);
		[[nodiscard]] bloop::BloopChar ReadEscapeCharacter();
		[[nodiscard]] bloop::BloopChar ReadHexCharacter();

		[[nodiscard]] bloop::EStatus ReadName(bloop::CToken& token) noexcept;
		[[nodiscard]] std::unique_ptr<bloop::CToken> ReadPunctuation() noexcept;


		bloop::BloopStringView::iterator m_oScriptPos;
		bloop::BloopStringView::iterator m_oLastScriptPos;
		bloop::BloopStringView::iterator m_oScriptEnd;

		bloop::CodePosition m_oParserPosition;
		bloop::BloopStringView m_sSource;

		std::vector<std::unique_ptr<bloop::CToken>> m_oTokens;
	};
}