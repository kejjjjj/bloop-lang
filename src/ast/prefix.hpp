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

	// !value: true for false, 0 and undefined (the same values that if () treats as false)
	struct LogicalNot final : Prefix {
		LogicalNot(const bloop::CodePosition& cp) : Prefix(bloop::EPunctuation::p_exclamation, cp) {}

		void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true);
			Emit(builder, TOpCode::NOT);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
	};

	struct PrefixIncrement final : Prefix {

		PrefixIncrement(const bloop::CodePosition& cp) : Prefix(EPunctuation::p_increment, cp) {}

		void Resolve(TResolver& resolver) override {
			ResolveStep(resolver, *left, BLOOPTEXT("increment"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitStep(builder, *this, *left, TOpCode::INCR, true, want_value);
		}
	};

	struct PrefixDecrement final : Prefix {

		PrefixDecrement(const bloop::CodePosition& cp) : Prefix(EPunctuation::p_decrement, cp) {}

		void Resolve(TResolver& resolver) override {
			ResolveStep(resolver, *left, BLOOPTEXT("decrement"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitStep(builder, *this, *left, TOpCode::DECR, true, want_value);
		}
	};

}