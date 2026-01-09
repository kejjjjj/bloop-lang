#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct Unary : BinaryExpression {
		Unary(bloop::EPunctuation punct, const bloop::CodePosition& cp) : BinaryExpression(punct, cp) {}
		[[nodiscard]] IdentifierExpression* GetIdentifier() noexcept override;
	};
}