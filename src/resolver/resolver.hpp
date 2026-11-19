#pragma once

#include "utils/defs.hpp"
#include "metadata/metadata.hpp"

#include <unordered_map>
#include <vector>
#include <memory>

namespace bloop::ast {
	struct Program;
	struct FunctionDeclarationStatement;
}

namespace bloop::resolver {

	void Resolve(bloop::ast::Program* code, bloop::metadata::Metadata& metadata);

	namespace internal {

		struct Symbol {
			bloop::BloopString m_sName;
			bloop::BloopInt m_iDepth{};
			bloop::BloopInt m_iFunctionDepth{};
			bloop::BloopIndex m_uSlot{};
			bloop::BloopBool m_bIsConst{};
			bloop::BloopBool m_bIsFunction{};
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
			bloop::BloopBool m_bConst{};
		};

		using DeclaredIdentifier = std::unordered_map<bloop::BloopString, bloop::BloopIndex>;

		// the names a module exports
		using ExportTable = std::unordered_map<bloop::BloopString, std::shared_ptr<Symbol>>;

		struct Resolver {

			Resolver(bloop::metadata::Metadata& metadata) : m_refMetadata(metadata){}

			// a program is resolved one module at a time (dependencies first):
			//   BeginProgram(), ResolveModule() for every module, EndProgram()
			// every module has its own global scope, but all globals live in one table
			void BeginProgram();
			void ResolveModule(bloop::ast::Program* code, bool isImported, ExportTable* exports);
			void EndProgram(bloop::ast::Program* code);

			// makes an exported symbol visible in the current module (read-only)
			void ImportSymbol(const bloop::BloopString& name, const Symbol& exported);

			std::vector<Scope> m_oScopes;
			std::vector<FunctionContext> m_oFunctions; // to keep track of local counts
			std::vector<DeclaredIdentifier> m_oDeclaredIdentifiers; // to avoid duplicate declarations
			bloop::BloopInt m_iLoopDepth{};
			bloop::BloopIndex m_uNextGlobalSlot{};
			bloop::BloopBool m_bInImportedModule{};
			bloop::BloopInt m_iScopeDepth{-1};
			bloop::metadata::Metadata& m_refMetadata;

			void InjectNatives();

			void BeginScope();
			void EndScope();

			[[maybe_unused]] Symbol* DeclareSymbol(const bloop::BloopString& name, bool isConst = false, bool isFunction=false);
			[[nodiscard]] Symbol* ResolveSymbol(const bloop::BloopString& name);
			[[nodiscard]] ResolvedIdentifier ResolveIdentifier(const bloop::BloopString& name);

			[[nodiscard]] bloop::BloopIndex CountLocals() const;

			[[nodiscard]] bloop::ast::FunctionDeclarationStatement* GetOuterMostFunction() const;
			std::vector<bloop::ast::FunctionDeclarationStatement*> m_oAllFunctions;

		private:
			[[nodiscard]] Symbol* ResolveLocal(const bloop::BloopString& name);
			[[nodiscard]] Symbol* ResolveGlobal(const bloop::BloopString& name);
			[[nodiscard]] Symbol* ResolveOuter(const bloop::BloopString& name);

		};
	}

}