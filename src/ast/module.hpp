#pragma once

#include "ast/ast.hpp"
#include "utils/fmt.hpp"

#include <vector>

namespace bloop::ast {

	// import a, b from "path";
	struct ImportStatement : Statement {
		ImportStatement(const bloop::CodePosition& cp) : Statement(cp) {}

		struct Name {
			bloop::BloopString m_sName;
			bloop::CodePosition m_oPosition;
		};

		void Resolve(TResolver& resolver) override {
			assert(m_pExports); // set by the module compiler

			for (const auto& [name, position] : m_oNames) {
				const auto itr = m_pExports->find(name);
				if (itr == m_pExports->end())
					throw bloop::exception::ResolverError(bloop::fmt::format(BLOOPTEXT("\"{}\" doesn't export \"{}\""), m_sPath, name), position);

				if (resolver.ResolveSymbol(name))
					throw bloop::exception::ResolverError(bloop::fmt::format(BLOOPTEXT("identifier \"{}\" already declared"), name), position);

				resolver.ImportSymbol(name, *itr->second);
			}
		}

		// the imported module has already run by the time this statement is reached
		void EmitByteCode([[maybe_unused]] TBCBuilder& builder) override {}

		std::vector<Name> m_oNames;
		bloop::BloopString m_sPath; // as written in the code

		// the exports of the imported module, filled in before it's resolved
		const bloop::resolver::internal::ExportTable* m_pExports{};
	};

}
