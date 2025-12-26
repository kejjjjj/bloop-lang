#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct FunctionCall : BinaryExpression {

		FunctionCall(const bloop::CodePosition& cp) : BinaryExpression(EPunctuation::p_par_open, cp){}
		FunctionCall(std::vector<std::unique_ptr<Expression>>&& args, const bloop::CodePosition& cp) 
			: BinaryExpression(EPunctuation::p_par_open, cp), m_oArguments(std::forward<decltype(args)>(args)) {}

		virtual void Resolve(TResolver& resolver) override {
			for (auto& arg : m_oArguments)
				arg->Resolve(resolver);
			
			left->Resolve(resolver);

		}
		virtual void EmitByteCode(TBCBuilder& builder) override {
			for (auto& arg : m_oArguments)
				arg->EmitByteCode(builder);

			left->EmitByteCode(builder);
			builder.Emit(TOpCode::CALL, static_cast<bloop::BloopUInt16>(m_oArguments.size()));
		}

		std::vector<std::unique_ptr<Expression>> m_oArguments;
	};

}