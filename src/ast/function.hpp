#pragma once

#include "ast/ast.hpp"
#include "utils/fmt.hpp"
#include <iostream>

namespace bloop::ast {

	struct Capture {
		enum class Kind { Local, Upvalue } kind{};
		bloop::BloopIndex m_uSlot{};

		constexpr bloop::bytecode::vmdata::Capture ToBC() const noexcept {
			return { kind == Kind::Local, m_uSlot };
		}
	};
	using Symbol = bloop::resolver::internal::Symbol;

	struct FunctionDeclarationStatement : Statement {
		FunctionDeclarationStatement(const BloopString& name, std::vector<BloopString>&& params,
			std::unique_ptr<BlockStatement>&& body, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_oParams(std::forward<decltype(params)>(params)), m_pBody(std::forward<decltype(body)>(body)) {
		}
		[[nodiscard]] constexpr bool IsFunction() const noexcept override { return true; }

		void Resolve(TResolver& resolver) override {

			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("already declared: ") + m_sName, m_oApproximatePosition);
			}

			resolver.DeclareSymbol(m_sName, true);
			m_oIdentifier = resolver.ResolveIdentifier(m_sName);

			if(resolver.m_oAllFunctions.size() >= bloop::INVALID_SLOT)
				throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("the code has more than {} functions"), bloop::INVALID_SLOT), m_oApproximatePosition);

			m_uFunctionId = static_cast<bloop::BloopIndex>(resolver.m_oAllFunctions.size());
			resolver.m_oAllFunctions.push_back(this);

			resolver.m_oFunctions.push_back({0, this });
			resolver.BeginScope();
			m_iScopeDepth = resolver.m_iScopeDepth;

			std::ranges::for_each(m_oParams, [this, &resolver](const std::string& param) {
				if (resolver.ResolveSymbol(param))
					throw bloop::exception::ResolverError(BLOOPTEXT("already declared: ") + param, m_oApproximatePosition);
				resolver.DeclareSymbol(param);
			});

			m_pBody->ResolveNoScopeManagement(resolver);
			m_uLocalCount = resolver.m_oFunctions.back().m_uNextSlot;
			resolver.EndScope();
			resolver.m_oFunctions.pop_back();
		}

		void PrintInstructions(TBCBuilder& parent) {
			std::cout << bloop::fmt::format(BLOOPTEXT("\n{}: (id: {})\n"), m_sName, m_uFunctionId);
			parent.Print();
		}

		using FunctionContext = bloop::resolver::internal::FunctionContext;
		using CaptureT = std::unordered_map<const Symbol*, bloop::BloopIndex>;
		void PropagateCaptureInward(FunctionDeclarationStatement* prevFunc, std::list<FunctionContext>& nextFunctions, const Symbol* symbol) {

			if(!m_uNextUpValues)
				m_uNextUpValues = std::make_unique<CaptureT>();

			const auto AddLocal = [&] { 
				(*m_uNextUpValues)[symbol] = static_cast<bloop::BloopIndex>(m_uNextUpValues->size());
				m_oCaptures.push_back({ Capture::Kind::Local, symbol->m_uSlot });
			};

			const auto AddUpvalue = [&](bloop::BloopIndex slot) {
				(*m_uNextUpValues)[symbol] = static_cast<bloop::BloopIndex>(m_uNextUpValues->size());
				m_oCaptures.push_back({ Capture::Kind::Upvalue, slot });
			};

			if (prevFunc) {
				const auto& theseVals = prevFunc->m_uNextUpValues;
				if (theseVals->contains(symbol)) {
					if (!m_uNextUpValues->contains(symbol))
						AddUpvalue(theseVals->at(symbol));
				} else {
					AddLocal();
				}
			} else if (!m_uNextUpValues->contains(symbol)) {
				AddLocal();
			}

			nextFunctions.pop_front();
			if(!nextFunctions.empty())
				return nextFunctions.front().m_pCurrentFunction->PropagateCaptureInward(this, nextFunctions, symbol);
		}

		void EmitByteCode(TBCBuilder& parent) override;

		bloop::BloopString m_sName;
		std::vector<BloopString> m_oParams;
		std::unique_ptr<BlockStatement> m_pBody;

		bloop::BloopIndex m_uFunctionId{ 0 };
		bloop::BloopIndex m_uLocalCount{ 0 };
		bloop::BloopInt m_iScopeDepth{ 0 };

		bloop::resolver::internal::ResolvedIdentifier m_oIdentifier{};

		std::vector<Capture> m_oCaptures;
		std::unique_ptr<CaptureT> m_uNextUpValues;
	};

}