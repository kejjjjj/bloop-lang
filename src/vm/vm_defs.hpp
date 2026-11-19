#pragma once

#include <vector>

#include "vm/value.hpp"
#include "bytecode/bc_common.hpp"

namespace bloop::vm
{
	struct Chunk {
		bloop::BloopIndex m_uMetadata{};
		std::vector<Value> m_oConstants;
	};
	struct Function : bc::Function {
		Chunk chunk;
	};

	struct ExceptionHandler {
		bloop::BloopSize m_uIp{};
		bloop::BloopSize m_uBase{};
		bloop::BloopIndex m_uCatchVar{};
	};
}