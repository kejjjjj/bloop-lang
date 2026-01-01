#pragma once

#include "ast/ast.hpp"
#include "utils/fmt.hpp"
#include <iostream>

namespace bloop::ast {

	struct Capture {
		enum class Kind { Local, Upvalue } kind{};
		bloop::BloopUInt16 m_uSlot{};

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

			m_uFunctionId = static_cast<bloop::BloopUInt16>(resolver.m_oAllFunctions.size());
			resolver.m_oAllFunctions.push_back(this);

			resolver.m_oFunctions.push_back({0, this });
			resolver.BeginScope();

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
			std::cout << bloop::fmt::format("\n{}: (id: {})\n", m_sName, m_uFunctionId);
			parent.Print();
		}

		using FunctionContext = bloop::resolver::internal::FunctionContext;
		using CaptureT = std::unordered_map<const Symbol*, Capture>;
		void PropagateCaptureInward(FunctionDeclarationStatement* prevFunc , std::list<FunctionContext>& nextFunctions, const Symbol* symbol) {

			if (prevFunc) {
				assert(prevFunc->m_oCaptures.contains(symbol));
				auto oldSize = static_cast<bloop::BloopUInt16>(prevFunc->m_oCaptures.size());
				auto& cap = prevFunc->m_oCaptures[symbol] = prevFunc->m_oCaptures.at(symbol);

				//transform into an upvalue when it escapes the local scope
				if (cap.kind == Capture::Kind::Local) {
					cap.kind = Capture::Kind::Upvalue;
					cap.m_uSlot = oldSize;
				}
			} else {
				m_oCaptures[symbol] = { Capture::Kind::Local, symbol->m_uSlot };
			}

			nextFunctions.pop_front();
			if(!nextFunctions.empty())
				return nextFunctions.front().m_pCurrentFunction->PropagateCaptureInward(this, nextFunctions, symbol);
		}

		void EmitByteCode(TBCBuilder& parent) override;

		bloop::BloopString m_sName;
		std::vector<BloopString> m_oParams;
		std::unique_ptr<BlockStatement> m_pBody;

		bloop::BloopUInt16 m_uFunctionId{ 0 };
		bloop::BloopUInt16 m_uLocalCount{ 0 };

		bloop::resolver::internal::ResolvedIdentifier m_oIdentifier{};

		//when creating CAPTURE_LOCAL / CAPTURE_UPVALUE
		CaptureT m_oCaptures;

		//when creating LOAD_UPVALUE
		CaptureT m_oUpValues;
	};

}