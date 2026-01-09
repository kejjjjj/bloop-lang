#pragma once

#include "ast/unary.hpp"

namespace bloop::ast {

	struct Prefix : Unary {
		Prefix(EPunctuation punct, const bloop::CodePosition& cp) : Unary(punct, cp) {}
	};

	struct Negation : Prefix {
		Negation(const bloop::CodePosition& cp) : Prefix(bloop::EPunctuation::p_sub, cp) {}

		void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true); // load operand
			Emit(builder, TOpCode::NEG);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

	};

	struct PrefixIncrement final : Prefix {

		PrefixIncrement(const bloop::CodePosition& cp) : Prefix(EPunctuation::p_increment, cp) {}

		void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
			if (const auto identifier = GetIdentifier()) {
				if (identifier->IsConst())
					throw exception::ResolverError(BLOOPTEXT("can't increment a const value"), m_oApproximatePosition);
				return;
			}
			throw exception::ResolverError(BLOOPTEXT("increment operand is not an identifier"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true); // load operand

			Emit(builder, TOpCode::INCR);

			if (want_value)
				Emit(builder, TOpCode::DUP); //duplicate

			auto identifier = GetIdentifier();
			assert(identifier);
			identifier->Store(builder, !want_value); //invert it

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
	};

	struct PrefixDecrement final : Prefix {

		PrefixDecrement(const bloop::CodePosition& cp) : Prefix(EPunctuation::p_decrement, cp) {}

		void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);

			if (const auto identifier = GetIdentifier()) {
				if (identifier->IsConst())
					throw exception::ResolverError(BLOOPTEXT("can't decrement a const value"), m_oApproximatePosition);
				return;
			}
			throw exception::ResolverError(BLOOPTEXT("decrement operand is not an identifier"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true); // load operand

			Emit(builder, TOpCode::DECR);

			if (want_value)
				Emit(builder, TOpCode::DUP); //duplicate

			auto identifier = GetIdentifier();
			assert(identifier);
			identifier->Store(builder, !want_value); //invert it

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
	};

}