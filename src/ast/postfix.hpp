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

				auto expr = dynamic_cast<Postfix*>(_left);

				if (!expr)
					break;

				_left = expr->left.get();

			}
			//assert(false);
			return nullptr;
		}

	};

	struct FunctionCall final : Postfix {

		FunctionCall(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_par_open, cp){}
		FunctionCall(std::vector<std::unique_ptr<Expression>>&& args, const bloop::CodePosition& cp) 
			: Postfix(EPunctuation::p_par_open, cp), m_oArguments(std::forward<decltype(args)>(args)) {}

		void Resolve(TResolver& resolver) override {
			for (auto& arg : m_oArguments)
				arg->Resolve(resolver);
			
			left->Resolve(resolver);

		}
		void EmitByteCode(TBCBuilder& builder, [[maybe_unused]] bool want_value) override {
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

	struct Decrement final : Postfix {

		Decrement(const bloop::CodePosition& cp) : Postfix(EPunctuation::p_decrement, cp) {}

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