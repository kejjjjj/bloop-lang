#pragma once

#include "bytecode/defs.hpp"
#include "utils/defs.hpp"

namespace bloop::ast {
	struct FunctionDeclarationStatement;
}

namespace bloop::bytecode {

	class CByteCodeFunction {
	public:
		CByteCodeFunction() = delete;
		CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl);

		void Generate(std::vector<vmdata::Function>& funcs);

	private:
		bloop::ast::FunctionDeclarationStatement* m_pFunc;
		bloop::BloopUInt m_uNumConsts{};
	};



}