#include "ast/unary.hpp"
#include "ast/prefix.hpp"
#include "ast/postfix.hpp"

using namespace bloop::ast;

IdentifierExpression* Unary::GetIdentifier() noexcept {

	auto _left = left.get();

	while (_left) {

		if (const auto identifier = _left->GetIdentifier())
			return identifier;

		BinaryExpression* expr = dynamic_cast<Prefix*>(_left);

		if (!expr)
			expr = dynamic_cast<Postfix*>(_left);

		if (!expr)
			break;

		_left = expr->left.get();

	}

	return nullptr;
}