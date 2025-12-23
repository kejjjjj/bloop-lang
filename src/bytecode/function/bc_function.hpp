#pragma once

#include "utils/defs.hpp"

namespace bloop::ast {
	struct FunctionDeclarationStatement;
}

namespace bloop::bytecode {

	class CByteCodeFunction {
	public:
		CByteCodeFunction() = delete;
		CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl);

		void Generate();

	private:
		bloop::ast::FunctionDeclarationStatement* m_pFunc;
		bloop::BloopUInt m_uNumConsts{};
	};

}