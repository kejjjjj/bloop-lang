#pragma once

#include "bytecode/defs.hpp"
#include "utils/defs.hpp"

#include <vector>

namespace bloop::ast {
	struct FunctionDeclarationStatement;
}

namespace bloop::bytecode {

	namespace vmdata {

		struct Function {
			bloop::BloopString m_sName;
			bloop::BloopUInt16 m_uParamCount{};
			bloop::BloopUInt16 m_uLocalCount{};
			std::vector<CConstant> m_oConstants;
			std::vector<BloopByte> m_oByteCode;
		};

	};

	class CByteCodeFunction {
	public:
		CByteCodeFunction() = delete;
		CByteCodeFunction(bloop::ast::FunctionDeclarationStatement* funcDecl);

		[[nodiscard]] vmdata::Function Generate();

	private:
		bloop::ast::FunctionDeclarationStatement* m_pFunc;
		bloop::BloopUInt m_uNumConsts{};
	};



}