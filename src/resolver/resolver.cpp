#include "resolver/resolver.hpp"
#include "ast/ast.hpp"

#include <cassert>
#include <ranges>

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
	auto slot = m_oFunctions.empty() ? static_cast<bloop::BloopInt>(scope.size()) : m_oFunctions.back().m_iNextSlot++;
	return &(scope[name] = Symbol{ name, m_iScopeDepth, slot, isConst });
}
Symbol* Resolver::ResolveSymbol(const bloop::BloopString& name) {
	
	const auto itr = std::ranges::find_if(m_oScopes.rbegin(), m_oScopes.rend(), [&name](Scope& s) { 
		return s.symbols.contains(name);
	});

	if (itr == m_oScopes.rend())
		return nullptr;

	return &itr->symbols.at(name);
}
