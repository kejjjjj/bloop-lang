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
			bloop::BloopIndex m_uSlot{};
			bloop::BloopBool m_bIsConst{};
		};

		struct Scope {

			std::unordered_map<bloop::BloopString, std::shared_ptr<Symbol>> symbols;
		};
		struct FunctionContext {
			bloop::BloopIndex m_uNextSlot = 0;
			bloop::ast::FunctionDeclarationStatement* m_pCurrentFunction{};



		};


		struct ResolvedIdentifier {
			enum class Kind { Error, Local, Upvalue, Global };
			Kind m_eKind;
			bloop::BloopIndex m_uSlot;
			bool m_bConst{};
		};
		struct Resolver {
			std::vector<Scope> m_oScopes;
			std::vector<FunctionContext> m_oFunctions; // to keep track of local counts
			bloop::BloopInt m_iLoopDepth{};
			bloop::BloopInt m_iScopeDepth{-1};

			void BeginScope();
			void EndScope();

			[[maybe_unused]] Symbol* DeclareSymbol(const bloop::BloopString& name, bool isConst = false);
			[[nodiscard]] Symbol* ResolveSymbol(const bloop::BloopString& name);
			[[nodiscard]] ResolvedIdentifier ResolveIdentifier(const bloop::BloopString& name);

			[[nodiscard]] bloop::ast::FunctionDeclarationStatement* GetOuterMostFunction() const;

			std::vector<bloop::ast::FunctionDeclarationStatement*> m_oAllFunctions;

		private:
			[[nodiscard]] Symbol* ResolveLocal(const bloop::BloopString& name);
			[[nodiscard]] Symbol* ResolveGlobal(const bloop::BloopString& name);
			[[nodiscard]] Symbol* ResolveOuter(const bloop::BloopString& name);

		};
	}

}