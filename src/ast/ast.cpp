#include "ast/ast.hpp"
#include "ast/postfix.hpp"

using namespace bloop::ast;

void AssignExpression::EmitByteCode(TBCBuilder& builder) {
	right->EmitByteCode(builder);

	//look for the identifier from the unary/postfix chain
	if (const auto ptr = left->GetIdentifier()) {

		if (auto pf = dynamic_cast<Subscript*>(left.get())) {
			pf->EmitSet(builder);
			return pf->left->EmitByteCode(builder);
		}

		Emit(builder, TOpCode::STORE_LOCAL, ptr->m_uSlot);
		//left->EmitByteCode(builder);
		return;
	}

	throw bloop::exception::ResolverError(BLOOPTEXT("lhs wasn't an identifier"), left->m_oApproximatePosition);

}