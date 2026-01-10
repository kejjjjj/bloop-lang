#pragma once

#include "ast/ast.hpp"
#include "utils/fmt.hpp"

namespace bloop::ast {

	struct TryCatch : Statement {
		TryCatch(const bloop::CodePosition& cp) : Statement(cp){}

		void Resolve(TResolver& resolver) override {
			m_pTryBlock->Resolve(resolver);
			m_pCatchVariable->Resolve(resolver);
			m_pCatchBlock->Resolve(resolver);
			m_uStackBase = resolver.CountLocals();

			if (m_uStackBase >= bloop::INVALID_SLOT)
				throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("more than {} locals in a chunk"), bloop::INVALID_SLOT));
		}
		void EmitByteCode(TBCBuilder& builder) override {

			const auto idx = builder.EmitTry(TOpCode::TRY, static_cast<bloop::BloopIndex>(m_uStackBase), m_oApproximatePosition);
			m_pTryBlock->EmitByteCode(builder);
			Emit(builder, TOpCode::TRY_END);
			builder.PatchTry(idx, builder.m_uOffset); //TRY jumps here on throw
			m_pCatchBlock->EmitByteCode(builder);
		}

		std::unique_ptr<BlockStatement> m_pTryBlock;
		std::unique_ptr<BlockStatement> m_pCatchBlock;
		std::unique_ptr<ConstVariableDeclaration> m_pCatchVariable; //resolver
		bloop::BloopUInt m_uStackBase{};
	};

}