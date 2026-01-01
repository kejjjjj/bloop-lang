#include "resolver/resolver.hpp"
#include "ast/function.hpp"

#include <cassert>
#include <ranges>
#include <list>

#define NOMINMAX

using namespace bloop::resolver;

void bloop::resolver::Resolve(bloop::ast::Program* code){
	internal::Resolver resolver;
	code->Resolve(resolver);
	code->m_uNumFunctions = resolver.m_oAllFunctions.size();
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
	auto& result = (scope[name] = std::make_shared<Symbol>(name, m_iScopeDepth, slot, isConst));
	return result.get();
}
Symbol* Resolver::ResolveSymbol(const bloop::BloopString& name) {
	
	//exit early if it exists in this scope

	if (auto* local = ResolveLocal(name))
		return local;

	if (auto* global = ResolveGlobal(name))
		return global;

	return ResolveOuter(name);
}
ResolvedIdentifier Resolver::ResolveIdentifier(const bloop::BloopString& name) {

	if (auto* sym = ResolveLocal(name))
		return { ResolvedIdentifier::Kind::Local, sym->m_uSlot };

	if (auto* sym = ResolveGlobal(name))
		return { ResolvedIdentifier::Kind::Global, sym->m_uSlot };

	if (auto* sym = ResolveOuter(name)) {

		auto funcs = m_oFunctions | std::views::drop(sym->m_iDepth);
		auto t = std::list<FunctionContext>(funcs.begin(), funcs.end());

		t.front().m_pCurrentFunction->PropagateCaptureInward(nullptr, t, sym);
		return ResolvedIdentifier{ ResolvedIdentifier::Kind::Upvalue, m_oFunctions.back().m_pCurrentFunction->m_uNextUpValues->at(sym)};
	}

	return ResolvedIdentifier{ ResolvedIdentifier::Kind::Error, {} };
}

Symbol* Resolver::ResolveLocal(const bloop::BloopString& name) {
	if (m_oScopes.back().symbols.contains(name))
		return m_oScopes.back().symbols.at(name).get();

	return nullptr;
}
Symbol* Resolver::ResolveGlobal(const bloop::BloopString& name) {
	if (m_oScopes.front().symbols.contains(name))
		return m_oScopes.front().symbols.at(name).get();
	return nullptr;
}
Symbol* Resolver::ResolveOuter(const bloop::BloopString& name)
{
	const auto itr = std::ranges::find_if(m_oScopes.rbegin(), m_oScopes.rend(), [&name](Scope& s) {
		return s.symbols.contains(name);
	});

	if (itr == m_oScopes.rend())
		return nullptr;

	return itr->symbols.at(name).get();
}

bloop::ast::FunctionDeclarationStatement* Resolver::GetOuterMostFunction() const {
	assert(!m_oFunctions.empty());
	return m_oFunctions.front().m_pCurrentFunction;
}