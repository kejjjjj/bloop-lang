#pragma once

#include <vector>

#include "bytecode/defs.hpp"

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode{
	namespace vmdata {
		struct Function;
	}



	[[nodiscard]] VMByteCode BuildByteCode(bloop::ast::Program* code);

}