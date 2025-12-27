#include "resolver/resolver.hpp"
#include "ast/function.hpp"

#include <cassert>
#include <ranges>

#define NOMINMAX

using namespace bloop::resolver;

void bloop::resolver::Resolve(bloop::ast::Program* code){
	internal::Resolver resolver;
	code->Resolve(resolver);
}

using namespace bloop::resolver::internal;

void Resolver::BeginScope() {
	m_oScopes.emplace_back();
	m_iScopeDepth++;
}
void Resolver::EndScope() {
	assert(!m_oScopes.empty());
	m_oScopes.pop_back();
	m_iScopeDepth--;
}
Symbol* Resolver::DeclareSymbol(const bloop::BloopString& name, bool isConst) {
	auto& scope = m_oScopes.back().symbols;

	if (!m_oFunctions.empty() && m_oFunctions.back().m_uNextSlot >= std::numeric_limits<bloop::BloopUInt16>::max())
		throw exception::ResolverError(BLOOPTEXT("too many locals in a function (most recent): ") + name);

	auto slot = m_oFunctions.empty() ? static_cast<bloop::BloopUInt16>(scope.size()) : m_oFunctions.back().m_uNextSlot++;
	return &(scope[name] = Symbol{ name, m_iScopeDepth, slot, isConst });
}
Symbol* Resolver::ResolveSymbol(const bloop::BloopString& name) {
	
	//exit early if it exists in this scope
	if (m_oScopes.back().symbols.contains(name))
		return &m_oScopes.back().symbols.at(name);

	const auto itr = std::ranges::find_if(m_oScopes.rbegin(), m_oScopes.rend(), [&name](Scope& s) {
		return s.symbols.contains(name);
	});

	if (itr == m_oScopes.rend())
		return nullptr;

	auto* symbol = &itr->symbols.at(name);
	m_oFunctions.back().m_pCurrentFunction->m_oCaptures.emplace_back(m_iScopeDepth - symbol->m_iDepth, symbol->m_uSlot);
	
	return symbol;
}
