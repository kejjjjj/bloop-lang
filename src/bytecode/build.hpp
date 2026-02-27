#pragma once

#include <vector>

#include "bytecode/defs.hpp"
#include "metadata/metadata.hpp"

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode{
	namespace vmdata {
		struct Function;
	}

	void BuildByteCode(bloop::ast::Program* code, bloop::metadata::Metadata& metadata);

}