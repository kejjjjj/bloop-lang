#include <cmath>
#include "lexer/lexer.hpp"
#include "lexer/exception.hpp"

#include <cassert>
#include <unordered_map>
#include <utility>
#include <iterator>
#include <cctype>

constexpr bool IsDigit(bloop::BloopChar c) noexcept
{
	return c >= '0' && c <= '9';
}
constexpr bool IsAlpha(bloop::BloopChar c) {

#ifdef UNICODE
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c > 127;
#else
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
#endif
}
constexpr auto IsHex(bloop::BloopChar c) -> bool
{
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

using namespace bloop::lexer;

CLexer::CLexer(bloop::BloopStringView buffer, bloop::metadata::Metadata& metadata)
	: m_sSource(buffer), m_refMetadata(metadata) {

	assert(buffer.data() && buffer.size());

	m_oScriptPos = m_sSource.begin();
	m_oLastScriptPos = m_sSource.end();
	m_oScriptEnd = m_sSource.end();
	m_oLineStart = m_oScriptPos;

	// when a program has several files, they all share one line map,
	// so the lines of this file continue from where the previous file ended
	m_oParserPosition = std::make_tuple(m_refMetadata.m_oLineMap.size() + 1u, size_t(1));

	assert(m_oScriptPos != m_oScriptEnd);

}
CLexer::~CLexer() = default;

void CLexer::Parse() {
	while (auto&& token = ReadToken()) {
		m_oTokens.emplace_back(std::forward<decltype(token)>(token));

		for (auto& pending : m_oPendingTokens)
			m_oTokens.emplace_back(std::move(pending));
		m_oPendingTokens.clear();
	}

	if (!m_oTemplateBraces.empty())
		throw exception::LexerError(BLOOPTEXT("a ${ in a template string isn't closed with }"), m_oParserPosition);

	// lines are recorded when their '\n' is reached, so the last line still needs to be added
	m_refMetadata.m_oLineMap.push_back(bloop::BloopStringView(m_oLineStart, m_oScriptEnd));
}

bool CLexer::IsToken(bloop::BloopStringView t) noexcept
{
	if ((std::size_t)std::distance(m_oScriptPos, m_oScriptEnd) < t.length())
		return false;

	const auto end = m_oScriptPos + (std::ptrdiff_t)t.length();
	const auto punctuation = bloop::BloopStringView(m_oScriptPos, end);
	return punctuation == t;
}

std::unique_ptr<bloop::CToken> CLexer::ReadToken()
{
	m_oLastScriptPos = m_oScriptPos;

	bloop::CToken token;

	if (EndOfBuffer())
		return nullptr;

	while (true) {

		if (ReadWhiteSpace() != bloop::EStatus::success)
			return nullptr;

		if (IsToken(BLOOPTEXT("//"))) {
			if (ReadSingleLineComment() != bloop::EStatus::success)
				return nullptr;
		} else if (IsToken(BLOOPTEXT("/*"))) {
			if (ReadMultiLineComment() != bloop::EStatus::success)
				return nullptr;
		} else {
			break;
		}
	}

	if (ReadWhiteSpace() != bloop::EStatus::success)
		return nullptr;

	token.m_oSourcePosition = m_oParserPosition;

	// template strings: `text ${expression} text`
	if (*m_oScriptPos == '`')
		return ReadTemplate(true);
	if (*m_oScriptPos == '}' && !m_oTemplateBraces.empty() && m_oTemplateBraces.back() == 0u)
		return ReadTemplate(false);

	if (IsDigit(*m_oScriptPos) || (*m_oScriptPos == '.' && IsDigit(*(std::next(m_oScriptPos))))) {
		if (ReadNumber(token) != bloop::EStatus::success) {
			return nullptr;
		}
	} else if (IsAlpha(*m_oScriptPos) || *m_oScriptPos == '_') {
		if (ReadName(token) != bloop::EStatus::success) {
			return nullptr;
		}
	} else if (*m_oScriptPos == '\"' || *m_oScriptPos == '\'') {
		if (ReadString(token, *m_oScriptPos) != bloop::EStatus::success) {
			return nullptr;
		}
	} else {
		if (auto punc = ReadPunctuation()) {
			// inside a substitution, only the } that matches its ${ ends it
			if (!m_oTemplateBraces.empty()) {
				if (punc->IsOperator(EPunctuation::p_curlybracket_open))
					m_oTemplateBraces.back()++;
				else if (punc->IsOperator(EPunctuation::p_curlybracket_close))
					m_oTemplateBraces.back()--; // (not 0: that } would have ended the substitution)
			}
			return punc;
		}

		throw exception::LexerError(BLOOPTEXT("a token without a definition"), m_oParserPosition);
	}

	return std::make_unique<bloop::CToken>(token);
}

bloop::EStatus CLexer::ReadWhiteSpace() noexcept
{
	if (EndOfBuffer())
		return bloop::EStatus::failure;

	auto& [line, column] = m_oParserPosition;

	while (*m_oScriptPos <= ' ') {

		if (EndOfBuffer())
			return bloop::EStatus::failure;

		if (*m_oScriptPos == '\n') {
			m_refMetadata.m_oLineMap.push_back(bloop::BloopStringView(m_oLineStart, m_oScriptPos));
			line++;
			column = size_t(1);
			m_oLineStart = std::next(m_oScriptPos);
		}
		else {
			if (*m_oScriptPos == '\t')
				column += size_t(4);
			else
				column++;
		}

		m_oScriptPos++;

		if (EndOfBuffer())
			return bloop::EStatus::failure;
	}

	return bloop::EStatus::success;
}
bloop::EStatus CLexer::ReadSingleLineComment() noexcept
{
	auto& [line, column] = m_oParserPosition;

	if (EndOfBuffer())
		return bloop::EStatus::failure;

	while (*m_oScriptPos != '\n') {

		if (*m_oScriptPos == '\t')
			column += size_t(4);
		else
			column++;

		m_oScriptPos++;

		if (EndOfBuffer())
			break;
	}

	return bloop::EStatus::success;
}
bloop::EStatus CLexer::ReadMultiLineComment()
{
	auto& [line, column] = m_oParserPosition;

	if (EndOfBuffer())
		return bloop::EStatus::failure;

	while (!IsToken(BLOOPTEXT("*/"))) {

		if (*m_oScriptPos == '\n') {
			m_refMetadata.m_oLineMap.push_back(bloop::BloopStringView(m_oLineStart, m_oScriptPos));
			line++;
			column = size_t(1);
			m_oLineStart = std::next(m_oScriptPos);
		} else {
			if (*m_oScriptPos == '\t')
				column += size_t(4);
			else
				column++;
		}

		m_oScriptPos++;

		if (EndOfBuffer()) {
			throw exception::LexerError(BLOOPTEXT("expected to find */ before EOF"));
		}
	}

	m_oScriptPos += 2; // */

	return bloop::EStatus::success;
}

bloop::EStatus CLexer::ReadNumber(bloop::CToken& token)
{
	auto& [_, column] = m_oParserPosition;

	if (*m_oScriptPos == '.') { //assumes that there is a number

		//if the character after the dot is not a number, then stop
		if (std::next(m_oScriptPos) == m_oScriptEnd || !IsDigit(*(std::next(m_oScriptPos)))) {
			return bloop::EStatus::failure;
		}

		token.m_sSource.push_back(*m_oScriptPos++);
		token.m_eTokenType = ETokenType::tt_double;

		//parse the integer literal after the .
		if (ReadInteger(token) != bloop::EStatus::success)
			return bloop::EStatus::failure;

		ReadExponent(token);
	}
	else if (IsDigit(*m_oScriptPos)) {
		token.m_eTokenType = ETokenType::tt_int;

		if (ReadInteger(token) != bloop::EStatus::success)
			return bloop::EStatus::failure;

		if (EndOfBuffer())
			return bloop::EStatus::success;

		const auto isFloat = *m_oScriptPos == '.';
		const auto isHex = token.m_sSource == BLOOPTEXT("0") && (*m_oScriptPos == 'x' || *m_oScriptPos == 'X');

		if (isHex) {
			m_oScriptPos++; // skip x
			if (ReadHex(token) != bloop::EStatus::success)
				return bloop::EStatus::failure;
		}
		//floating point decimal
		else if (isFloat) {
			token.m_sSource.push_back(*m_oScriptPos++);
			token.m_eTokenType = ETokenType::tt_double;

			//parse the integer literal after the .
			if (ReadInteger(token) != bloop::EStatus::success)
				return bloop::EStatus::failure;

		}

		// 1e6, 2.5e-3: an exponent makes the literal a double
		const auto hasExponent = !isHex && ReadExponent(token);

		//todo -> suffixes
		if (!isFloat && !hasExponent && !EndOfBuffer()) {
			switch (*m_oScriptPos) {
			case 'u':
			case 'U':
				token.m_eTokenType = ETokenType::tt_uint;
				m_oScriptPos++;
				break;
			default:
				break;
			}
		}
	}

	assert(token.m_sSource.length());

	column += token.m_sSource.length();
	return bloop::EStatus::success;
}
// e or E, an optional sign and digits after a number's digits; false (reading nothing) when there's no exponent
bool CLexer::ReadExponent(bloop::CToken& token)
{
	auto& [_, column] = m_oParserPosition;

	if (EndOfBuffer() || (*m_oScriptPos != 'e' && *m_oScriptPos != 'E'))
		return false;

	auto digit = std::next(m_oScriptPos);
	if (digit != m_oScriptEnd && (*digit == '+' || *digit == '-'))
		++digit;

	// 2.e or 1em: not an exponent, the e belongs to whatever comes next
	if (digit == m_oScriptEnd || !IsDigit(*digit)) {
		column += token.m_sSource.length() + 1;
		throw exception::LexerError(BLOOPTEXT("expected the exponent's digits after \"e\""), m_oParserPosition);
	}

	token.m_sSource.push_back('e');
	++m_oScriptPos;
	if (*m_oScriptPos == '+' || *m_oScriptPos == '-')
		token.m_sSource.push_back(*m_oScriptPos++);

	if (ReadInteger(token) != bloop::EStatus::success)
		throw exception::LexerError(BLOOPTEXT("expected the exponent's digits after \"e\""), m_oParserPosition);

	token.m_eTokenType = ETokenType::tt_double;

	if (!std::isfinite(std::strtod(token.m_sSource.c_str(), nullptr)))
		throw exception::LexerError(BLOOPTEXT("constant value is out of range"), m_oParserPosition);

	return true;
}

bloop::EStatus CLexer::ReadInteger(bloop::CToken& token)
{
	auto& [_, column] = m_oParserPosition;

	if (EndOfBuffer())
		return bloop::EStatus::failure;

	while (IsDigit(*m_oScriptPos)) {
		token.m_sSource.push_back(*m_oScriptPos++);

		if (EndOfBuffer())
			return bloop::EStatus::success;

		if (*m_oScriptPos == BLOOPTEXT('_')) {
			m_oScriptPos++;
			column++;

			if (EndOfBuffer() || !IsDigit(*m_oScriptPos)) {
				column += token.m_sSource.length();
				throw exception::LexerError(BLOOPTEXT("digit separator cannot end here"), m_oParserPosition);
			}

		}
	}


	return bloop::EStatus::success;

}
bloop::EStatus CLexer::ReadHex(bloop::CToken& token)
{
	auto& [_, column] = m_oParserPosition;

	if (EndOfBuffer())
		return bloop::EStatus::failure;

	bloop::BloopString hexStr;

	while (true) {

		if (EndOfBuffer()) {
			throw exception::LexerError(BLOOPTEXT("unexpected end of file when parsing hex"), m_oParserPosition);
		}

		if (!IsHex(*m_oScriptPos))
			break;

		hexStr.push_back(*m_oScriptPos++);
	}

	try {
		auto intValue = std::stoll(hexStr, nullptr, 16);
		token.m_sSource = std::to_string(intValue);
	}
	catch (std::out_of_range&) {
		try {
			const auto uintValue = std::stoull(hexStr, nullptr, 16);
			token.m_sSource = std::to_string(uintValue);
		}
		catch (std::out_of_range&) {
			throw exception::LexerError(BLOOPTEXT("constant value is out of range"), m_oParserPosition);
		}
	}
	column += hexStr.length();
	return bloop::EStatus::success;
}

bloop::EStatus CLexer::ReadString(bloop::CToken& token, bloop::BloopChar quote)
{
	auto& [line, column] = m_oParserPosition;

	token.m_eTokenType = ETokenType::tt_string;
	++m_oScriptPos;

	do {
		if (EndOfBuffer())
			return bloop::EStatus::failure;

		if (*m_oScriptPos == quote)
			break;

		if (*m_oScriptPos == '\n') {
			throw exception::LexerError(BLOOPTEXT("newline within a string"), m_oParserPosition);
		}
		else {
			column += (*m_oScriptPos == '\t' ? 4 : 1);
		}

		if (*m_oScriptPos == BLOOPTEXT('\\'))
			token.m_sSource.push_back(ReadEscapeCharacter());
		else
			token.m_sSource.push_back(*m_oScriptPos);

		m_oScriptPos++;

		if (EndOfBuffer())
			return bloop::EStatus::failure;

	} while (*m_oScriptPos != quote);

	m_oScriptPos++;  //skip "
	column++;

	return bloop::EStatus::success;
}
std::unique_ptr<bloop::CToken> CLexer::MakePunctuation(bloop::EPunctuation punctuation, const bloop::CodePosition& position) const {
	for (const auto& p : punctuations) {
		if (p.m_ePunctuation == punctuation) {
			auto token = std::make_unique<CPunctuationToken>(p);
			token->m_oSourcePosition = position;
			return token;
		}
	}
	assert(false);
	return nullptr;
}

// `a ${x} b ${y} c` becomes: head "a", (, x's tokens, ), middle " b ", (, y's tokens, ), tail " c".
// The parser parses each substitution like an expression in parentheses
std::unique_ptr<bloop::CToken> CLexer::ReadTemplate(bool start) {
	auto& [line, column] = m_oParserPosition;
	const auto position = m_oParserPosition;

	std::unique_ptr<bloop::CToken> closing;
	if (!start) {
		// the } ends the substitution
		m_oTemplateBraces.pop_back();
		closing = MakePunctuation(EPunctuation::p_par_close, position);
	}

	m_oScriptPos++; // ` or }
	column++;

	auto text = std::make_unique<bloop::CToken>();
	text->m_oSourcePosition = position;
	bool substitution{};

	while (true) {
		if (EndOfBuffer())
			throw exception::LexerError(BLOOPTEXT("the template string doesn't end (a ` is missing)"), position);

		const auto c = *m_oScriptPos;

		if (c == '`') {
			m_oScriptPos++;
			column++;
			break;
		}

		if (c == '$' && std::next(m_oScriptPos) != m_oScriptEnd && *std::next(m_oScriptPos) == '{') {
			m_oScriptPos += 2;
			column += 2u;
			substitution = true;
			break;
		}

		// template strings can span lines (a \r\n is a \n, like in js)
		if (c == '\r' && std::next(m_oScriptPos) != m_oScriptEnd && *std::next(m_oScriptPos) == '\n') {
			m_oScriptPos++;
			continue;
		}
		if (c == '\n') {
			m_refMetadata.m_oLineMap.push_back(bloop::BloopStringView(m_oLineStart, m_oScriptPos));
			line++;
			column = size_t(1);
			m_oLineStart = std::next(m_oScriptPos);
			text->m_sSource.push_back('\n');
			m_oScriptPos++;
			continue;
		}

		column += (c == '\t' ? 4 : 1);

		if (c == '\\')
			text->m_sSource.push_back(ReadEscapeCharacter()); // (leaves the position at the escaped character)
		else
			text->m_sSource.push_back(c);

		m_oScriptPos++;
	}

	if (start && !substitution)
		text->m_eTokenType = ETokenType::tt_string; // `no substitutions` is just a string
	else if (start)
		text->m_eTokenType = ETokenType::tt_template_head;
	else
		text->m_eTokenType = substitution ? ETokenType::tt_template_middle : ETokenType::tt_template_tail;

	if (substitution)
		m_oTemplateBraces.push_back(0u);

	// whatever comes after the token that this returns
	if (closing) {
		m_oPendingTokens.push_back(std::move(text));
		if (substitution)
			m_oPendingTokens.push_back(MakePunctuation(EPunctuation::p_par_open, m_oParserPosition));
		return closing;
	}

	if (substitution)
		m_oPendingTokens.push_back(MakePunctuation(EPunctuation::p_par_open, m_oParserPosition));
	return text;
}
bloop::BloopChar CLexer::ReadEscapeCharacter()
{
	auto& [line, column] = m_oParserPosition;

	m_oScriptPos++;  //skip '\\'
	column++;

	if (EndOfBuffer()) {
		throw exception::LexerError(BLOOPTEXT("unexpected end of file"), m_oParserPosition);
	}

	auto c = *m_oScriptPos;

	switch (c) {
	case '\\': c = '\\'; break;
	case 'n': c = '\n'; break;
	case 'r': c = '\r'; break;
	case 't': c = '\t'; break;
	case 'v': c = '\v'; break;
	case 'b': c = '\b'; break;
	case 'f': c = '\f'; break;
	case 'a': c = '\a'; break;
	case 'x': return ReadHexCharacter();
	case '\'': c = '\''; break;
	case '\"': c = '\"'; break;
	case '\?': c = '\?'; break;
	case '`': c = '`'; break;
	case '$': c = '$'; break; // (a template string's "\${" is text, not a substitution)
	default:
		throw exception::LexerError(BLOOPTEXT("unexpected escape character"), m_oParserPosition);
	}

	return c;

}
bloop::BloopChar CLexer::ReadHexCharacter()
{
	auto& [line, column] = m_oParserPosition;
	m_oScriptPos++;  //skip 'x'
	column++;

	if (EndOfBuffer()) {
		throw exception::LexerError(BLOOPTEXT("unexpected end of file"), m_oParserPosition);
	}

	if (!IsHex(*m_oScriptPos)) {
		throw exception::LexerError(BLOOPTEXT("invalid hex literal"), m_oParserPosition);
	}

	bloop::BloopString hexStr(1, *m_oScriptPos);
	m_oScriptPos++;  //skip first character
	column++;

	if (EndOfBuffer()) {
		throw exception::LexerError(BLOOPTEXT("unexpected end of file"), m_oParserPosition);
	}

	if (!IsHex(*m_oScriptPos)) {
		m_oScriptPos--;
		column--;
	} else {
		hexStr.push_back(*m_oScriptPos);
	}

	auto intValue = std::stoll(hexStr, nullptr, 16);
	return static_cast<bloop::BloopChar>(static_cast<unsigned char>(intValue)); //it's fine!!!!!
}

const std::unordered_map<bloop::BloopStringView, bloop::ETokenType> reservedKeywords = {
#define BLOOP_X(name) { BLOOPTEXT(#name), bloop::ETokenType::tt_##name },
#include "token_keywords.def"
#undef BLOOP_X
};

bloop::EStatus CLexer::ReadName(bloop::CToken& token) noexcept
{
	auto& [_, column] = m_oParserPosition;

	token.m_sSource.push_back(*m_oScriptPos++);
	token.m_eTokenType = ETokenType::tt_name;

	if (EndOfBuffer())
		return bloop::EStatus::success;

	while (std::isalnum(*m_oScriptPos) || *m_oScriptPos == '_') {
		token.m_sSource.push_back(*m_oScriptPos++);
		if (EndOfBuffer())
			break;
	}

	if (reservedKeywords.contains(token.m_sSource)) {
		token.m_eTokenType = reservedKeywords.at(token.m_sSource);
	}

	column += token.m_sSource.length();
	return bloop::EStatus::success;
}
std::unique_ptr<bloop::CToken> CLexer::ReadPunctuation() noexcept
{
	auto& [line, column] = m_oParserPosition;

	for (const auto& i : punctuations) {
		if (static_cast<size_t>(std::distance(m_oScriptPos, m_oScriptEnd)) >= i.m_sIdentifier.length()) {
			const auto end = m_oScriptPos + static_cast<ptrdiff_t>(i.m_sIdentifier.length());
			const auto punctuation = bloop::BloopStringView(m_oScriptPos, end);

			if (punctuation.empty())
				return nullptr;

			if (!punctuation.compare(i.m_sIdentifier)) {
				auto token = std::make_unique<CPunctuationToken>(i);
				token->m_oSourcePosition = std::tie(line, column);

				column += token->m_sSource.length();
				m_oScriptPos += static_cast<ptrdiff_t>(token->m_sSource.length());
				return token;
			}
		}

	}


	return nullptr;
}
