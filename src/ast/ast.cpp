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

		switch (ptr->m_oResolver.m_eKind) {
		case IdentifierExpression::ResolvedIdentifier::Kind::Local:
			Emit(builder, TOpCode::STORE_LOCAL, ptr->m_oResolver.m_uSlot);
			break;
		case IdentifierExpression::ResolvedIdentifier::Kind::Upvalue:
			Emit(builder, TOpCode::STORE_UPVALUE, ptr->m_oResolver.m_uSlot);
			break;
		case IdentifierExpression::ResolvedIdentifier::Kind::Global:
			Emit(builder, TOpCode::STORE_GLOBAL, ptr->m_oResolver.m_uSlot);
			break;
		}

		return;
	}

	throw bloop::exception::ResolverError(BLOOPTEXT("lhs wasn't an identifier"), left->m_oApproximatePosition);

}