#pragma once

#include "utils/defs.hpp"

#include <unordered_map>
#include <vector>

namespace bloop::ast {
	struct Program;
	struct FunctionDeclarationStatement;
}

namespace bloop::resolver {

	void Resolve(bloop::ast::Program* code);

	namespace internal {

		struct Symbol {
			bloop::BloopString m_sName;
			bloop::BloopInt m_iDepth{};
			bloop::BloopUInt16 m_uSlot{};
			bloop::BloopBool m_bIsConst{};
			bloop::BloopBool m_bIsUpValue{};
		};

		struct Scope {
			std::unordered_map<bloop::BloopString, Symbol> symbols;
		};
		struct FunctionContext {
			bloop::BloopUInt16 m_uNextSlot = 0;
			bloop::ast::FunctionDeclarationStatement* m_pCurrentFunction{};
		};
		struct Resolver {
			std::vector<Scope> m_oScopes;
			std::vector<FunctionContext> m_oFunctions; // to keep track of local counts

			bloop::BloopInt m_iScopeDepth{-1};
			void BeginScope();
			void EndScope();

			[[maybe_unused]] Symbol* DeclareSymbol(const bloop::BloopString& name, bool isConst = false);
			[[nodiscard]] Symbol* ResolveSymbol(const bloop::BloopString& name);

			std::vector<bloop::ast::FunctionDeclarationStatement*> m_oAllFunctions;
		};
	}

}