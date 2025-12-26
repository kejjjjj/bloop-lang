#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct FunctionCall : Expression {

		FunctionCall(const bloop::CodePosition& cp) : Expression(cp){}
		FunctionCall(std::vector<std::unique_ptr<Expression>>&& args, const bloop::CodePosition& cp) 
			: Expression(cp), m_oArguments(std::forward<decltype(args)>(args)) {}

		virtual void Resolve(TResolver& resolver) override {
			for (auto& arg : m_oArguments)
				arg->Resolve(resolver);

		}
		virtual void EmitByteCode(TBCBuilder& builder) override {
			for (auto& arg : m_oArguments)
				arg->EmitByteCode(builder);
		}

		std::vector<std::unique_ptr<Expression>> m_oArguments;
	};

}