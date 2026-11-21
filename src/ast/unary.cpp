#include "ast/unary.hpp"
#include "ast/prefix.hpp"
#include "ast/postfix.hpp"
#include "utils/fmt.hpp"

#include <cassert>

using namespace bloop::ast;

IdentifierExpression* Unary::GetIdentifier() noexcept {

	auto _left = left.get();

	while (_left) {

		if (const auto identifier = _left->GetIdentifier())
			return identifier;

		// through the prefixes and postfixes around it
		BinaryExpression* expr = _left->AsUnary();

		if (!expr)
			break;

		_left = expr->left.get();

	}

	return nullptr;
}

void bloop::ast::ResolveStep(TResolver& resolver, Expression& operand, const bloop::BloopString& verb, const bloop::CodePosition& position) {
	operand.Resolve(resolver);

	if (operand.AsSubscript() || operand.AsPropertyAccess())
		return;

	if (auto* identifier = operand.AsIdentifier()) {
		if (identifier->IsConst())
			throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("can't {} a const value"), verb), position);
		return;
	}

	throw exception::ResolverError(bloop::fmt::format(BLOOPTEXT("{} operand must be a variable, an element or a property"), verb), position);
}

void bloop::ast::EmitStep(TBCBuilder& builder, const AbstractSyntaxTree& self, Expression& operand, TOpCode op, bool prefix, bool want_value) {

	operand.EmitByteCode(builder, true); // the current value

	auto* subscript = operand.AsSubscript();
	auto* property = operand.AsPropertyAccess();

	if (subscript || property) {
		// x++ evaluates to the old value: keep a copy under the new one
		if (!prefix && want_value)
			self.Emit(builder, TOpCode::DUP);

		self.Emit(builder, op);

		// leaves the stored (new) value on the stack
		if (subscript)
			subscript->EmitSet(builder, true);
		else
			property->EmitSet(builder, true);

		if (!want_value || !prefix)
			self.Emit(builder, TOpCode::POP);
		return;
	}

	auto* identifier = operand.AsIdentifier();
	assert(identifier);

	if (!prefix && want_value)
		self.Emit(builder, TOpCode::DUP);

	self.Emit(builder, op);

	if (prefix && want_value)
		self.Emit(builder, TOpCode::DUP);

	identifier->Store(builder, !want_value); // keeps the stored value on the stack only when nothing else is

	if (!want_value)
		self.Emit(builder, TOpCode::POP);
}
