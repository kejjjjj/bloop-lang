#pragma once

#include "bytecode/defs.hpp"
#include "utils/defs.hpp"
#include "metadata/metadata.hpp"

#include <vector>

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode {

	class CByteCodeGlobals {
	public:
		CByteCodeGlobals() = delete;
		CByteCodeGlobals(bloop::ast::Program* code);

		[[nodiscard]] bc::Chunk Generate(bloop::metadata::Metadata& metadata);

	private:
		bloop::ast::Program* m_pCode;
	};
}