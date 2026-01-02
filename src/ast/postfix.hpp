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
		virtual void EmitByteCode(TBCBuilder& builder) override {
			for (auto& arg : m_oArguments)
				arg->EmitByteCode(builder); // load args

			left->EmitByteCode(builder); // load operand
			Emit(builder, TOpCode::CALL, static_cast<bloop::BloopIndex>(m_oArguments.size()));
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
		virtual void EmitByteCode(TBCBuilder& builder) override {
			EmitGet(builder);
		}
		void EmitGet(TBCBuilder& builder) {
			left->EmitByteCode(builder);   // arr
			m_pIndex->EmitByteCode(builder); // index
			Emit(builder, TOpCode::SUBSCRIPT_GET);
		}

		void EmitSet(TBCBuilder& builder) {
			left->EmitByteCode(builder);   // arr
			m_pIndex->EmitByteCode(builder); // index
			Emit(builder, TOpCode::SUBSCRIPT_SET);
		}

		std::unique_ptr<Expression> m_pIndex;
	};

}