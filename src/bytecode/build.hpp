#pragma once

#include <vector>

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode{
	namespace vmdata {
		struct Function;
	}

	[[nodiscard]] std::vector<vmdata::Function> BuildByteCode(bloop::ast::Program* code);

}