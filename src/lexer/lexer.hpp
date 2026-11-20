#pragma once

#include "lexer/token.hpp"
#include "metadata/metadata.hpp"

#include <memory>
#include <vector>

namespace bloop::lexer {

	class CLexer final {
	public:
		CLexer(bloop::BloopStringView buffer, bloop::metadata::Metadata& metadata);
		~CLexer();

		void Parse();
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
		// at a ` (a new template string), or at the } that closes a substitution (the rest of the template)
		[[nodiscard]] std::unique_ptr<bloop::CToken> ReadTemplate(bool start);
		[[nodiscard]] std::unique_ptr<bloop::CToken> MakePunctuation(bloop::EPunctuation punctuation, const bloop::CodePosition& position) const;
		[[nodiscard]] bloop::BloopChar ReadEscapeCharacter();
		[[nodiscard]] bloop::BloopChar ReadHexCharacter();

		[[nodiscard]] bloop::EStatus ReadName(bloop::CToken& token) noexcept;
		[[nodiscard]] std::unique_ptr<bloop::CToken> ReadPunctuation() noexcept;


		bloop::BloopStringView::iterator m_oScriptPos;
		bloop::BloopStringView::iterator m_oLastScriptPos;
		bloop::BloopStringView::iterator m_oScriptEnd;
		bloop::BloopStringView::iterator m_oLineStart;

		bloop::CodePosition m_oParserPosition;
		bloop::BloopStringView m_sSource;

		std::vector<std::unique_ptr<bloop::CToken>> m_oTokens;
		// tokens that ReadToken made in addition to the one it returned (template strings make several at once)
		std::vector<std::unique_ptr<bloop::CToken>> m_oPendingTokens;
		// one per substitution (${ ... }) that's open: the number of { inside it that aren't closed yet,
		// so that only its own } ends it
		std::vector<bloop::BloopSize> m_oTemplateBraces;

		bloop::metadata::Metadata& m_refMetadata;
	};
}