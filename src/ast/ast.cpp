#include "ast/ast.hpp"
#include "ast/postfix.hpp"

using namespace bloop::ast;

void AssignExpression::EmitByteCode(TBCBuilder& builder, bool want_value) {
	right->EmitByteCode(builder, true);

	//look for the identifier from the unary/postfix chain
	if (const auto ptr = left->GetIdentifier()) {

		if (auto pf = dynamic_cast<FunctionCall*>(left.get()))
			throw exception::ResolverError(BLOOPTEXT("invalid lhs operand"), m_oApproximatePosition);

		if (auto pf = dynamic_cast<Subscript*>(left.get())) {
			pf->EmitSet(builder, want_value);
			pf->EmitGet(builder, want_value); // (arr[0] = 2) < 10
			pf->left->EmitByteCode(builder, want_value);
		} else {
			ptr->Store(builder, true);
		}

		if (!want_value)
			Emit(builder, TOpCode::POP);

		return;
	}

	throw bloop::exception::ResolverError(BLOOPTEXT("lhs wasn't an identifier"), left->m_oApproximatePosition);

}
void AssignExpression::Resolve(TResolver& resolver) {
	BinaryExpression::Resolve(resolver);

	if (auto pf = dynamic_cast<FunctionCall*>(left.get()))
		throw exception::ResolverError(BLOOPTEXT("can't assign function calls"), m_oApproximatePosition);

	if (left->IsConst())
		throw bloop::exception::ResolverError(BLOOPTEXT("lhs is declared as const"), left->m_oApproximatePosition);

}