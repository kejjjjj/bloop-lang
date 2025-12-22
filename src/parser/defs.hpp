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
		[[nodiscard]] virtual UniqueStatement ToAST() = 0;
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

#if defined(_WIN32)
#if defined(_WIN64)
// 64-bit Windows
using BloopInt = long long;
using BloopUInt = unsigned long long;
#else
// 32-bit Windows
using BloopInt = int;
using BloopUInt = unsigned int;
#endif
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if defined(__x86_64__) || defined(__aarch64__)
// 64-bit macOS
using BloopInt = long long;
using BloopUInt = unsigned long long;
#else
// 32-bit macOS
using BloopInt = int;
using BloopUInt = unsigned int;
#endif
#else
#if defined(__x86_64__) || defined(__ppc64__)
// 64-bit non-Windows, non-macOS
using BloopInt = long long;
using BloopUInt = unsigned long long;
#else
// 32-bit non-Windows, non-macOS
using BloopInt = int;
using BloopUInt = unsigned int;
#endif
#endif

using BloopDouble = double;
