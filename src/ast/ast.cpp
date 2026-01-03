#include "ast/ast.hpp"
#include "ast/postfix.hpp"

using namespace bloop::ast;

void AssignExpression::EmitByteCode(TBCBuilder& builder) {
	right->EmitByteCode(builder);

	//look for the identifier from the unary/postfix chain
	if (const auto ptr = left->GetIdentifier()) {

		if (auto pf = dynamic_cast<FunctionCall*>(left.get()))
			throw exception::ResolverError(BLOOPTEXT("invalid lhs operand"), m_oApproximatePosition);

		if (auto pf = dynamic_cast<Subscript*>(left.get())) {
			pf->EmitSet(builder);
			if (!IsStatement())
				pf->EmitGet(builder); // (arr[0] = 2) < 10
			return pf->left->EmitByteCode(builder);
		}

		switch (ptr->m_oResolver.m_eKind) {
		case IdentifierExpression::ResolvedIdentifier::Kind::Local:
			Emit(builder, TOpCode::STORE_LOCAL, ptr->m_oResolver.m_uSlot);
			if(!IsStatement())
				Emit(builder, TOpCode::LOAD_LOCAL, ptr->m_oResolver.m_uSlot);
			break;
		case IdentifierExpression::ResolvedIdentifier::Kind::Upvalue:
			Emit(builder, TOpCode::STORE_UPVALUE, ptr->m_oResolver.m_uSlot);
			if (!IsStatement())
				Emit(builder, TOpCode::LOAD_UPVALUE, ptr->m_oResolver.m_uSlot);
			break;
		case IdentifierExpression::ResolvedIdentifier::Kind::Global:
			Emit(builder, TOpCode::STORE_GLOBAL, ptr->m_oResolver.m_uSlot);
			if (!IsStatement())
				Emit(builder, TOpCode::LOAD_GLOBAL, ptr->m_oResolver.m_uSlot);
			break;
		}

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