#include "resolver/resolver.hpp"
#include "ast/function.hpp"
#include "std/native.hpp"

#include <cassert>
#include <ranges>
#include <list>
#include <iterator>
#include <algorithm>

#define NOMINMAX

using namespace bloop::resolver;

void bloop::resolver::Resolve(bloop::ast::Program* code, bloop::metadata::Metadata& metadata) {
	internal::Resolver resolver(metadata);
	resolver.BeginProgram();
	resolver.ResolveModule(code, false, nullptr);
	resolver.EndProgram(code);
}

using namespace bloop::resolver::internal;

void Resolver::BeginProgram() {
	m_oDeclaredIdentifiers.push_back({});
	BeginScope(); // the natives live in the outermost scope, which every module can see
	InjectNatives();
}

void Resolver::ResolveModule(bloop::ast::Program* code, bool isImported, ExportTable* exports) {
	assert(m_oScopes.size() == 1u && m_oFunctions.empty());

	m_bInImportedModule = isImported;

	// the module's own global scope: modules can't see each other's globals unless they're imported,
	// so two modules can both have a global with the same name (each still gets its own slot)
	m_oDeclaredIdentifiers.push_back({});
	BeginScope();

	code->ResolveNoScopeManagement(*this);

	if (exports) {
		for (const auto& stmt : code->m_oStatements) {
			if (!stmt->m_bExported)
				continue;

			const auto* name = stmt->GetDeclaredName();
			assert(name && m_oScopes.back().symbols.contains(*name));
			(*exports)[*name] = m_oScopes.back().symbols.at(*name);
		}
	}

	EndScope();
	m_oDeclaredIdentifiers.pop_back();
	m_bInImportedModule = false;
}

void Resolver::EndProgram(bloop::ast::Program* code) {
	EndScope();

	auto& metadata = m_refMetadata;
	for (auto& func : metadata.m_oFunctionDebugInfo) {

		bloop::BloopString funcName = func.m_sName;

		static_assert(1u < bloop::INVALID_SLOT);
		for (const auto i : std::views::iota(1u, bloop::INVALID_SLOT)) {
			if (metadata.m_oFunctionTable.contains(funcName)) {
				funcName = func.m_sName + '#' + std::to_string(i);
				continue;
			}
			break;
		}

		//would be funny if someone just defined the same function like 65k times in different scopes
		if (metadata.m_oFunctionTable.contains(funcName))
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the function \"{}\" appears more than {} times"), func.m_sName, bloop::INVALID_SLOT));

		metadata.m_oFunctionTable[funcName] = &func;
	}

	code->m_uNumFunctions = static_cast<bloop::BloopIndex>(m_oAllFunctions.size());
	code->m_uNumGlobals = m_uNextGlobalSlot;
	code->m_oAllFunctions = std::move(m_oAllFunctions);

	m_oDeclaredIdentifiers.pop_back();
}

void Resolver::ImportSymbol(const bloop::BloopString& name, const Symbol& exported) {
	assert(m_oFunctions.empty());
	// same slot as the exporter's variable, so the importer sees its current value. Imports can't be assigned to
	m_oScopes.back().symbols[name] = std::make_shared<Symbol>(name, m_iScopeDepth, bloop::BloopInt{}, exported.m_uSlot, true, exported.m_bIsFunction);
}



void Resolver::InjectNatives() {
	if (m_refMetadata.m_oNatives.empty())
		m_refMetadata.m_oNatives = bloop::standard::g_Natives;

	for (const auto& stdObj : m_refMetadata.m_oNatives) {
		if(ResolveSymbol(stdObj.m_sName))
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the native module \"{}\" is defined more than once"), stdObj.m_sName));

		DeclareSymbol(stdObj.m_sName, true, true);
	}
}

void Resolver::BeginScope() {
	m_oScopes.emplace_back();
	m_iScopeDepth++;
}
void Resolver::EndScope() {
	assert(!m_oScopes.empty());
	m_oScopes.pop_back();
	m_iScopeDepth--;
}
Symbol* Resolver::DeclareSymbol(const bloop::BloopString& name, bool isConst, bool isFunction) {
	auto& scope = m_oScopes.back().symbols;

	bloop::BloopIndex slot{};
	bloop::BloopInt funcDepth{};

	if (!m_oFunctions.empty())
		funcDepth = m_oFunctions.back().m_pCurrentFunction->m_iScopeDepth;

	if (m_oDeclaredIdentifiers.back().contains(name)) {
		slot = m_oDeclaredIdentifiers.back().at(name); //give it the same slot
	} else if (m_oFunctions.empty()) {

		// every distinct global name gets its own slot. (Counting the symbols that are currently in scope
		// isn't enough: that count drops when a block ends, while names seen before keep their old slots,
		// so two different variables could end up sharing a slot). The counter is shared by all modules
		if (m_uNextGlobalSlot >= bloop::INVALID_SLOT)
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the code has more than {} globals"), bloop::INVALID_SLOT));
		slot = m_uNextGlobalSlot++;

		//slot = static_cast<bloop::BloopIndex>(scope.size());
		m_oDeclaredIdentifiers.back()[name] = slot; //to avoid redeclarations

	} else {
		if (m_oFunctions.back().m_uNextSlot >= bloop::INVALID_SLOT) {
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the function \"{}\" has more than {} symbols"),
				m_oFunctions.back().m_pCurrentFunction->m_sName, bloop::INVALID_SLOT));
		}
		slot = m_oFunctions.back().m_uNextSlot++;
		m_oDeclaredIdentifiers.back()[name] = slot; //to avoid redeclarations
	}
	auto& result = (scope[name] = std::make_shared<Symbol>(name, m_iScopeDepth, funcDepth, slot, isConst, isFunction));
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
		return { ResolvedIdentifier::Kind::Local, sym->m_uSlot, sym->m_bIsConst };

	if (auto* sym = ResolveGlobal(name)) {
		if (sym->m_bCapturedByValue && !m_oFunctions.empty()) {
			// every function between the top level and here captures it
			auto t = std::list<FunctionContext>(m_oFunctions.begin(), m_oFunctions.end());
			t.front().m_pCurrentFunction->PropagateCaptureInward(nullptr, t, sym);
			return ResolvedIdentifier{
				ResolvedIdentifier::Kind::Upvalue,
				m_oFunctions.back().m_pCurrentFunction->m_uNextUpValues->at(sym),
				sym->m_bIsConst
			};
		}
		return { ResolvedIdentifier::Kind::Global, sym->m_uSlot, sym->m_bIsConst };
	}

	if (auto* sym = ResolveOuter(name)) {

		// the functions nested inside the one that owns the symbol (m_iFunctionDepth is the scope depth of that function)
		const auto owner = std::ranges::find_if(m_oFunctions, [sym](const FunctionContext& f) {
			return f.m_pCurrentFunction->m_iScopeDepth == sym->m_iFunctionDepth; });
		assert(owner != m_oFunctions.end());
		auto funcs = m_oFunctions | std::views::drop(std::distance(m_oFunctions.begin(), owner) + 1);
		auto t = std::list<FunctionContext>(funcs.begin(), funcs.end());

		assert(!t.empty());

		t.front().m_pCurrentFunction->PropagateCaptureInward(nullptr, t, sym);
		return ResolvedIdentifier{ 
			ResolvedIdentifier::Kind::Upvalue, 
			m_oFunctions.back().m_pCurrentFunction->m_uNextUpValues->at(sym),
			sym->m_bIsConst
		};
	}

	return ResolvedIdentifier{ ResolvedIdentifier::Kind::Error, {}, false };
}

Symbol* Resolver::ResolveLocal(const bloop::BloopString& name) {
	//if (m_oScopes.back().symbols.contains(name))
	//	return m_oScopes.back().symbols.at(name).get();

	if (m_oFunctions.empty())
		return nullptr;

	auto& func = m_oFunctions.back().m_pCurrentFunction;
	auto range = std::vector<Scope>(m_oScopes.begin() + static_cast<std::ptrdiff_t>(func->m_iScopeDepth), m_oScopes.end());

	if (const auto itr = std::ranges::find_if(range.rbegin(), range.rend(), [&name](Scope& s) {
		return s.symbols.contains(name); }); itr != range.rend()) {
		return itr->symbols.at(name).get();
	}

	return nullptr;
}
Symbol* Resolver::ResolveGlobal(const bloop::BloopString& name) {

	if (m_oFunctions.empty()) {
		const auto itr = std::ranges::find_if(m_oScopes.rbegin(), m_oScopes.rend(), [&name](Scope& s) {
			return s.symbols.contains(name);
		});

		if (itr == m_oScopes.rend())
			return nullptr;

		return itr->symbols.at(name).get();
	}

	auto& func = m_oFunctions.front().m_pCurrentFunction;
	auto range = std::vector<Scope>(m_oScopes.begin(), m_oScopes.begin() + static_cast<std::ptrdiff_t>(func->m_iScopeDepth));

	if (const auto itr = std::ranges::find_if(range.rbegin(), range.rend(), [&name](Scope& s) {
		return s.symbols.contains(name); }); itr != range.rend()) {
		return itr->symbols.at(name).get();
	}

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

bloop::BloopIndex Resolver::CountLocals() const
{
	if (m_oFunctions.empty()) {
		const auto result = std::accumulate(m_oScopes.begin(), m_oScopes.end(), bloop::BloopUInt{}, [](auto sum, const Scope& s) {
			return sum + s.symbols.size();
		});

		assert(result < bloop::INVALID_SLOT);
		return static_cast<bloop::BloopIndex>(result);
	}

	return m_oFunctions.back().m_uNextSlot;
}
bloop::ast::FunctionDeclarationStatement* Resolver::GetOuterMostFunction() const {
	assert(!m_oFunctions.empty());
	return m_oFunctions.front().m_pCurrentFunction;
}