#pragma once

#include "ast/ast.hpp"
#include "utils/fmt.hpp"

namespace bloop::ast {

	struct TryCatch : Statement {
		TryCatch(const bloop::CodePosition& cp) : Statement(cp){}

		[[nodiscard]] constexpr bool IsDeclaration() const noexcept override { return true; }

		void Resolve(TResolver& resolver) override {
			//m_uStackBase = resolver.CountLocals();
			m_pTryBlock->Resolve(resolver);

			resolver.BeginScope();

			m_pCatchVariable->Resolve(resolver);
			m_pCatchBlock->ResolveNoScopeManagement(resolver);

			resolver.EndScope();

			//if (m_uStackBase >= bloop::INVALID_SLOT)
			//	throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("more than {} locals in a chunk"), bloop::INVALID_SLOT));
		}
		void EmitByteCode(TBCBuilder& builder) override {

			const auto idx = builder.EmitTry(TOpCode::TRY, m_pCatchVariable->m_uSlot, m_oApproximatePosition);
			m_pTryBlock->EmitByteCode(builder);
			Emit(builder, TOpCode::TRY_END);
			auto jumpIdx = EmitJump(builder, TOpCode::JMP); // jump to the position after the catch
			builder.PatchTry(idx, builder.m_uOffset); //TRY jumps here on throw
			m_pCatchBlock->EmitByteCode(builder);
			PatchJump(builder, jumpIdx, builder.m_uOffset); // try end goes here
		}

		std::unique_ptr<BlockStatement> m_pTryBlock;
		std::unique_ptr<BlockStatement> m_pCatchBlock;
		std::unique_ptr<ConstVariableDeclaration> m_pCatchVariable; //resolver
		//bloop::BloopIndex m_uStackBase{}; //calculated during runtime
	};


	struct ThrowStatement : ExpressionStatement {

		ThrowStatement(std::unique_ptr<Expression>&& expr, const bloop::CodePosition& cp) :
			ExpressionStatement(std::forward<decltype(expr)>(expr), cp) {
		}

		void Resolve(TResolver& resolver) override {
			assert(m_pExpression);
			ExpressionStatement::Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			m_pExpression->EmitByteCode(builder, true);
			Emit(builder, TOpCode::THROW);
		}
	};

}