#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct Unary : BinaryExpression {
		Unary(bloop::EPunctuation punct, const bloop::CodePosition& cp) : BinaryExpression(punct, cp) {}
		[[nodiscard]] IdentifierExpression* GetIdentifier() noexcept override;
	};

	// ++ and -- (prefix and postfix). The operand is a variable, an element (arr[i]) or a property (obj.key).
	// Changing an element or a property of a const array/object is fine, only the variable itself is const
	void ResolveStep(TResolver& resolver, Expression& operand, const bloop::BloopString& verb, const bloop::CodePosition& position);
	void EmitStep(TBCBuilder& builder, const AbstractSyntaxTree& self, Expression& operand, TOpCode op, bool prefix, bool want_value);
}