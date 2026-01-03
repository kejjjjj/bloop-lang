#pragma once


#pragma once
#include "parser/defs.hpp"
#include "utils/defs.hpp"
#include "parser/control/control.hpp"

namespace bloop {
	enum class ETokenType : unsigned char;
}

namespace bloop::parser {
	struct CParserContext;

	class CParserControlStatement : CParserStatement {
		BLOOP_NONCOPYABLE(CParserControlStatement);

		enum class Type {
			cf_error,
			cf_continue,
			cf_break
		} type{ Type::cf_error };

	public:
		CParserControlStatement() = delete;
		CParserControlStatement(const CParserContext& ctx);
		~CParserControlStatement() = default;

		[[nodiscard]] bloop::EStatus Parse();

		[[nodiscard]] UniqueStatement ToStatement() override;

	private:
	};
}