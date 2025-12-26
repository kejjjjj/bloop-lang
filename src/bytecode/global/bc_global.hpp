#pragma once

#include "bytecode/defs.hpp"
#include "utils/defs.hpp"

#include <vector>

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode {

	class CByteCodeGlobals {
	public:
		CByteCodeGlobals() = delete;
		CByteCodeGlobals(bloop::ast::Program* code);

		[[nodiscard]] vmdata::Chunk Generate();

	private:
		bloop::ast::Program* m_pCode;
	};
}