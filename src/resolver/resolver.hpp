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
			bloop::BloopInt m_iSlot{};
		};

		struct Scope {
			std::unordered_map<bloop::BloopString, Symbol> symbols;
		};

		struct Resolver {
			std::vector<Scope> m_oScopes;
			bloop::BloopInt m_iScopeDepth{};
			bloop::BloopUInt m_uNumFunctions{};
			void BeginScope();
			void EndScope();

			[[maybe_unused]] Symbol* DeclareSymbol(const bloop::BloopString& name);
			[[nodiscard]] Symbol* ResolveSymbol(const bloop::BloopString& name);

		};
	}

}