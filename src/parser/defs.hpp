#pragma once

#include <vector>
#include <memory>

namespace bloop {
	class CPunctuationToken;
	class CToken;
}
namespace bloop::ast {
	struct Statement;
	struct Expression;
}

namespace bloop::parser {

	using UniqueStatement = std::unique_ptr<bloop::ast::Statement>;
	using UniqueExpression = std::unique_ptr<bloop::ast::Expression>;

	struct IStatement {
		virtual ~IStatement() = default;
	protected:
		[[nodiscard]] virtual UniqueStatement ToStatement() = 0;
	};

	using ParserIterator = std::vector<bloop::CToken*>::iterator;

	class CParser {
	public:
		CParser() = delete;
		explicit CParser(ParserIterator& pos, ParserIterator& end) : m_iterPos(pos), m_iterEnd(end) {}
		virtual ~CParser() = default;

		[[nodiscard]] constexpr bool IsEndOfBuffer() const noexcept { return m_iterPos == m_iterEnd; }
		[[nodiscard]] constexpr auto GetIteratorSafe() { return IsEndOfBuffer() ? *std::prev(m_iterPos) : *m_iterPos; }

		constexpr void Advance(std::ptrdiff_t amount) const noexcept { std::advance(m_iterPos, amount); }


	protected:
		ParserIterator& m_iterPos;
		ParserIterator& m_iterEnd;
	};

	template<class Type>
	class CParserSingle : public CParser
	{
	public:
		CParserSingle() = delete;
		explicit CParserSingle(ParserIterator& pos, ParserIterator& end) : CParser(pos, end) {}
		CParserSingle operator=(const CParserSingle&) = delete;

	protected:
		const Type* m_pToken{};
	};
}
