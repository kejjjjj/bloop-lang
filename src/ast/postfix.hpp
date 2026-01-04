#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct Postfix : BinaryExpression {

		Postfix(EPunctuation punct, const bloop::CodePosition& cp) : BinaryExpression(punct, cp) {}

		[[nodiscard]] IdentifierExpression* GetIdentifier() noexcept override {

			auto _left = left.get();

			while (_left) {

				if (const auto identifier = _left->GetIdentifier())
					return identifier;

				auto expr = dynamic_cast<BinaryExpression*>(_left);

				if (!expr)
					break;

				_left = expr->left.get();
			}
			//assert(false);
			return nullptr;
		}

	};

	struct FunctionCall : Postfix {

		FunctionCall(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_par_open, cp){}
		FunctionCall(std::vector<std::unique_ptr<Expression>>&& args, const bloop::CodePosition& cp) 
			: Postfix(EPunctuation::p_par_open, cp), m_oArguments(std::forward<decltype(args)>(args)) {}

		virtual void Resolve(TResolver& resolver) override {
			for (auto& arg : m_oArguments)
				arg->Resolve(resolver);
			
			left->Resolve(resolver);

		}
		virtual void EmitByteCode(TBCBuilder& builder, [[maybe_unused]] bool want_value) override {
			for (auto& arg : m_oArguments)
				arg->EmitByteCode(builder, true); // load args

			left->EmitByteCode(builder, true); // load operand
			Emit(builder, TOpCode::CALL, static_cast<bloop::BloopIndex>(m_oArguments.size()));

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		std::vector<std::unique_ptr<Expression>> m_oArguments;
	};

	struct Subscript : Postfix {

		Subscript(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_bracket_open, cp) {}
		Subscript(std::unique_ptr<Expression>&& index, const bloop::CodePosition& cp)
			: Postfix(EPunctuation::p_bracket_open, cp), m_pIndex(std::forward<decltype(index)>(index)) {
		}

		virtual void Resolve(TResolver& resolver) override {
			m_pIndex->Resolve(resolver);
			left->Resolve(resolver);

		}
		virtual void EmitByteCode(TBCBuilder& builder, bool want_value) override {
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


	struct Increment : Postfix {

		Increment(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_increment, cp) {}

		virtual void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
			if (const auto identifier = GetIdentifier()) {
				return;
			}

			throw exception::ResolverError(BLOOPTEXT("increment operand is not an identifier"), m_oApproximatePosition);
		}
		virtual void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true); // load operand

			if(want_value)
				Emit(builder, TOpCode::DUP); //duplicate

			Emit(builder, TOpCode::INCR);

			auto identifier = GetIdentifier();
			assert(identifier);
			identifier->Store(builder, !want_value); //invert it

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
	};

	struct Decrement : Postfix {

		Decrement(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_decrement, cp) {}

		virtual void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);

			if (const auto identifier = GetIdentifier()) {
				return;
			}

			throw exception::ResolverError(BLOOPTEXT("decrement operand is not an identifier"), m_oApproximatePosition);
		}
		virtual void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true); // load operand

			if (want_value)
				Emit(builder, TOpCode::DUP); //duplicate

			Emit(builder, TOpCode::DECR);

			auto identifier = GetIdentifier();
			assert(identifier);
			identifier->Store(builder, !want_value); //invert it

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
	};

}