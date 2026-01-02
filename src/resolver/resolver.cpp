#include "resolver/resolver.hpp"
#include "ast/function.hpp"

#include <cassert>
#include <ranges>
#include <list>

#define NOMINMAX

using namespace bloop::resolver;

void bloop::resolver::Resolve(bloop::ast::Program* code){
	internal::Resolver resolver;

	//clear out any silly business
	for (auto& globalStatement : code->m_oStatements) {
		if (dynamic_cast<bloop::ast::UnnamedScopeStatement*>(globalStatement.get()))
			throw exception::ResolverError(BLOOPTEXT("unnamed scopes aren't allowed in the global scope"), globalStatement->m_oApproximatePosition);

	}

	if(resolver.m_oAllFunctions.size() >= bloop::INVALID_SLOT)
		throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the code has more than {} functions"), bloop::INVALID_SLOT));

	code->Resolve(resolver);
	code->m_uNumFunctions = static_cast<bloop::BloopIndex>(resolver.m_oAllFunctions.size());
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

	if (!m_oFunctions.empty() && m_oFunctions.back().m_uNextSlot >= std::numeric_limits<bloop::BloopIndex>::max())
		throw exception::ResolverError(BLOOPTEXT("too many locals in a function (most recent): ") + name);

	bloop::BloopIndex slot{};
	if (m_oFunctions.empty()) {
		if (scope.size() >= bloop::INVALID_SLOT)
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the code has more than {} globals"), bloop::INVALID_SLOT));

		slot = static_cast<bloop::BloopIndex>(scope.size());
	} else {
		if (m_oFunctions.back().m_uNextSlot >= bloop::INVALID_SLOT) {
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the function \"{}\" has more than {} symbols"),
				m_oFunctions.back().m_pCurrentFunction->m_sName, bloop::INVALID_SLOT));
		}
		slot = m_oFunctions.back().m_uNextSlot++;
	}
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
	//if (m_oScopes.back().symbols.contains(name))
	//	return m_oScopes.back().symbols.at(name).get();

	if (m_oFunctions.empty())
		return nullptr;

	auto& func = m_oFunctions.back().m_pCurrentFunction;
	auto range = std::vector<Scope>(m_oScopes.begin() + func->m_iScopeDepth, m_oScopes.end());

	if (const auto itr = std::ranges::find_if(range.rbegin(), range.rend(), [&name](Scope& s) {
		return s.symbols.contains(name); }); itr != range.rend()) {
		return itr->symbols.at(name).get();
	}

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