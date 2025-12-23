#pragma once

namespace bloop::ast {
	struct Program;
}

namespace bloop::bytecode{
	void BuildByteCode(bloop::ast::Program* code);

}