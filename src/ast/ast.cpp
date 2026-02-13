#include "ast/ast.hpp"
#include "ast/postfix.hpp"
#include "ast/function.hpp"

using namespace bloop::ast;

void BlockStatement::ResolveStatements(TResolver& resolver) {

	std::ranges::for_each(m_oStatements, [&](const std::unique_ptr<Statement>& s) {


		if (s->IsFunction()) {
			const auto asFunc = dynamic_cast<FunctionDeclarationStatement*>(s.get());

			if (auto sym = resolver.ResolveSymbol(asFunc->m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("already declared: ") + asFunc->m_sName, asFunc->m_oApproximatePosition);
			}

			resolver.DeclareSymbol(asFunc->m_sName, true, true);
		}
	});

	std::ranges::for_each(m_oStatements, [&](const auto& s) {

		//if (returnFound)
		//	throw exception::ResolverError(BLOOPTEXT("unreachable code"), s->m_oApproximatePosition);

		//if (s->IsReturn())
		//	returnFound = true;

		s->Resolve(resolver);
	});
	
}

void AssignExpression::EmitByteCode(TBCBuilder& builder, bool want_value) {
	right->EmitByteCode(builder, true);

	//look for the identifier from the unary/postfix chain
	if (const auto ptr = left->GetIdentifier()) {

		if (auto pf = dynamic_cast<FunctionCall*>(left.get()))
			throw exception::ResolverError(BLOOPTEXT("invalid lhs operand"), m_oApproximatePosition);

		if (auto pf = dynamic_cast<Subscript*>(left.get())) {
			pf->EmitSet(builder, true);
			if(want_value)
				pf->EmitGet(builder, true); // (arr[0] = 2) < 10
			//pf->left->EmitByteCode(builder, true);
		} else if (auto pa = dynamic_cast<PropertyAccess*>(left.get())) {
			pa->EmitSet(builder, true);
			if (want_value)
				pa->EmitGet(builder, true); // (obj.prop = 2) < 10
			//pa->left->EmitByteCode(builder, true);
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

FunctionExpression::FunctionExpression(std::unique_ptr<FunctionDeclarationStatement>&& funcDecl, const bloop::CodePosition& cp)
	: Expression(cp), m_pFuncDecl(std::forward<decltype(funcDecl)>(funcDecl)) {}

FunctionExpression::~FunctionExpression() = default;

void FunctionExpression::Resolve(TResolver& resolver) {
	m_pFuncDecl->Resolve(resolver);
}
void FunctionExpression::EmitByteCode(TBCBuilder& builder, bool want_value) {

	builder.m_bIsLambda = true;
	m_pFuncDecl->EmitByteCode(builder);
	builder.m_bIsLambda = false;

	if (!want_value)
		Emit(builder, TOpCode::POP);
}