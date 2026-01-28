#pragma once

#include "utils/defs.hpp"

#include <vector>

namespace bloop::vm {

	struct Value;
	class VM;

	namespace native {
		using NativeFn = Value(*)(VM& vm, const std::vector<Value>& args);

		struct NativeDef {
			bloop::BloopString m_sName;
			bloop::BloopIndex m_uParamCount;
			NativeFn m_pFunction;
		};

		extern std::vector<NativeDef> g_Natives;
	}
}