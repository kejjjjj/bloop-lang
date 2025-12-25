#pragma once

#include "utils/defs.hpp"

#include <unordered_map>
#include <vector>

namespace bloop::ast {
	struct Program;
}

namespace bloop::resolver {

	void Resolve(bloop::ast::Program* code);

	namespace internal {

		struct Symbol {
			bloop::BloopString m_sName;
			bloop::BloopInt m_iDepth{};
			bloop::BloopUInt16 m_uSlot{};
			bloop::BloopBool m_bIsConst{};
		};

		struct Scope {
			std::unordered_map<bloop::BloopString, Symbol> symbols;
		};
		struct FunctionContext {
			bloop::BloopUInt16 m_uNextSlot = 0;
		};
		struct Resolver {
			std::vector<Scope> m_oScopes;
			std::vector<FunctionContext> m_oFunctions; // to keep track of local counts

			bloop::BloopInt m_iScopeDepth{};
			bloop::BloopUInt m_uNumFunctions{};
			void BeginScope();
			void EndScope();

			[[maybe_unused]] Symbol* DeclareSymbol(const bloop::BloopString& name, bool isConst = false);
			[[nodiscard]] Symbol* ResolveSymbol(const bloop::BloopString& name);

		};
	}

}