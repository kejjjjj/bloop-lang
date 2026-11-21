#pragma once

#include "ast/unary.hpp"

namespace bloop::ast {

	struct Postfix : Unary {
		Postfix(EPunctuation punct, const bloop::CodePosition& cp) : Unary(punct, cp) {}
	};

	struct FunctionCall final : Postfix {

		FunctionCall(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_par_open, cp){}
		[[nodiscard]] FunctionCall* AsFunctionCall() noexcept override { return this; }
		FunctionCall(std::vector<std::unique_ptr<Expression>>&& args, const bloop::CodePosition& cp) 
			: Postfix(EPunctuation::p_par_open, cp), m_oArguments(std::forward<decltype(args)>(args)) {}

		void Resolve(TResolver& resolver) override {
			for (auto& arg : m_oArguments)
				arg->Resolve(resolver);
			
			left->Resolve(resolver);

		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			for (auto& arg : m_oArguments)
				arg->EmitByteCode(builder, true); // load args

			left->EmitByteCode(builder, true); // load operand
			Emit(builder, TOpCode::CALL, static_cast<bloop::BloopIndex>(m_oArguments.size()));

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		std::vector<std::unique_ptr<Expression>> m_oArguments;
	};

	struct Subscript final : Postfix {

		Subscript(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_bracket_open, cp) {}
		[[nodiscard]] Subscript* AsSubscript() noexcept override { return this; }
		Subscript(std::unique_ptr<Expression>&& index, const bloop::CodePosition& cp)
			: Postfix(EPunctuation::p_bracket_open, cp), m_pIndex(std::forward<decltype(index)>(index)) {
		}

		void Resolve(TResolver& resolver) override {
			m_pIndex->Resolve(resolver);
			left->Resolve(resolver);

		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitGet(builder, want_value);
		}
		void EmitGet(TBCBuilder& builder, bool want_value) {
			left->EmitByteCode(builder, true);   // arr
			m_pIndex->EmitByteCode(builder, true); // index
			Emit(builder, TOpCode::SUBSCRIPT_GET);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		void EmitSet(TBCBuilder& builder, bool want_value) {
			left->EmitByteCode(builder, true);   // arr
			m_pIndex->EmitByteCode(builder, true); // index
			Emit(builder, TOpCode::SUBSCRIPT_SET);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		std::unique_ptr<Expression> m_pIndex;
	};

	struct PropertyAccess final : Postfix {
		PropertyAccess(bloop::ConstantData _const, const bloop::CodePosition& cp) 
			: Postfix(EPunctuation::p_period, cp), m_oConstant(_const) {}
		[[nodiscard]] PropertyAccess* AsPropertyAccess() noexcept override { return this; }

		void Resolve(TResolver& resolver) override {
			assert(std::get<0>(m_oConstant).empty() == false);

			left->Resolve(resolver);
			return;
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitGet(builder, want_value);
		}

		void EmitGet(TBCBuilder& builder, bool want_value) {
			left->EmitByteCode(builder, true);   // obj
			Emit(builder, TOpCode::LOAD_CONST, builder.AddConstant(m_oConstant));
			Emit(builder, TOpCode::PROPERTY_GET);
			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
		void EmitSet(TBCBuilder& builder, bool want_value) {
			left->EmitByteCode(builder, true);   // obj
			Emit(builder, TOpCode::LOAD_CONST, builder.AddConstant(m_oConstant));
			Emit(builder, TOpCode::PROPERTY_SET);
			if (!want_value)
				Emit(builder, TOpCode::POP);

		}
		bloop::ConstantData m_oConstant;
	};

	struct Increment final : Postfix {

		Increment(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_increment, cp) {}

		void Resolve(TResolver& resolver) override {
			ResolveStep(resolver, *left, BLOOPTEXT("increment"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitStep(builder, *this, *left, TOpCode::INCR, false, want_value);
		}
	};

	struct Decrement final : Postfix {

		Decrement(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_decrement, cp) {}

		void Resolve(TResolver& resolver) override {
			ResolveStep(resolver, *left, BLOOPTEXT("decrement"), m_oApproximatePosition);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			EmitStep(builder, *this, *left, TOpCode::DECR, false, want_value);
		}
	};

}