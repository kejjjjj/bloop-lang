#pragma once

#include "utils/defs.hpp"
#include "lexer/token.hpp"
#include "resolver/resolver.hpp"
#include "resolver/exception.hpp"
#include "bytecode/compile/emit.hpp"
#include "bytecode/exception.hpp"

#include "utils/fmt.hpp"

#include <memory>
#include <vector>
#include <utility>
#include <optional>

namespace bloop::ast {
	using TResolver = bloop::resolver::internal::Resolver;
	using TBCBuilder = bloop::bytecode::CByteCodeBuilder;
	using TOpCode = bloop::bytecode::EOpCode;

	struct AbstractSyntaxTree	{
		AbstractSyntaxTree(bloop::CodePosition cp) : m_oApproximatePosition(cp){}
		virtual ~AbstractSyntaxTree() = default;
		bloop::CodePosition m_oApproximatePosition;

		inline void Emit(TBCBuilder& builder, TOpCode insn) const {
			builder.Emit(insn, { m_oApproximatePosition });
		}
		inline void Emit(TBCBuilder& builder, TOpCode insn, bloop::BloopIndex arg) const {
			builder.Emit(insn, arg, { m_oApproximatePosition });
		}
		inline void Emit(TBCBuilder& builder, TOpCode insn, bloop::BloopIndex arg, bloop::BloopIndex arg2) const {
			builder.Emit(insn, arg, arg2, { m_oApproximatePosition });
		}
		inline void Emit(TBCBuilder& builder, TOpCode insn, bloop::BloopIndex arg, bloop::BloopIndex arg2, bloop::BloopIndex arg3) const {
			builder.Emit(insn, arg, arg2, arg3, { m_oApproximatePosition });
		}
		inline void Omit(TBCBuilder& builder) {
			builder.Omit();
		}
		[[nodiscard]] inline bloop::BloopIndex EmitJump(TBCBuilder& builder, TOpCode opcode) {
			return builder.EmitJump(opcode, {m_oApproximatePosition});
		}
		inline void EmitJump(TBCBuilder& builder, TOpCode opcode, bloop::BloopIndex offset) {
			builder.EmitJump(opcode, offset, { m_oApproximatePosition });

		}
		inline void PatchJump(TBCBuilder& builder, bloop::BloopIndex src, bloop::BloopIndex dst) {
			builder.PatchJump(src, dst);
		}

	};

	struct Statement : AbstractSyntaxTree {
		Statement(bloop::CodePosition cp) : AbstractSyntaxTree(cp){}
		virtual void Resolve(TResolver& resolver) = 0;
		virtual void EmitByteCode(TBCBuilder& builder) = 0;
		[[nodiscard]] virtual constexpr bool IsFunction() const noexcept { return false; }
		[[nodiscard]] virtual constexpr bool IsReturn() const noexcept { return false; }
		[[nodiscard]] virtual constexpr bool IsDeclaration() const noexcept { return false; }
	};

	struct BlockStatement : Statement {
		BlockStatement(const bloop::CodePosition& cp) : Statement(cp){}


		virtual void Resolve(TResolver& resolver) override {
			resolver.BeginScope();
			ResolveStatements(resolver);
			resolver.EndScope();
		}

		void ResolveNoScopeManagement(TResolver& resolver) {
			ResolveStatements(resolver);
		}

		virtual void EmitByteCode(TBCBuilder& builder) override {
			std::ranges::for_each(m_oStatements, [&builder](const auto& s) { s->EmitByteCode(builder); });
		}

		void AddStatement(std::unique_ptr<Statement>&& stmt) {
			m_oStatements.emplace_back(std::forward<decltype(stmt)>(stmt));
		}
		
		std::vector<std::unique_ptr<Statement>> m_oStatements;

	private:
		void ResolveStatements(TResolver& resolver);
	};
	struct UnnamedScopeStatement : BlockStatement {
		UnnamedScopeStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}
	};

	struct IdentifierExpression;
	struct Expression : AbstractSyntaxTree {
		Expression(const bloop::CodePosition& cp) : AbstractSyntaxTree(cp) {}

		virtual void Resolve(TResolver& resolver) = 0;
		virtual void EmitByteCode(TBCBuilder& builder, bool want_value) = 0;
		[[nodiscard]] virtual constexpr bool IsConst() const noexcept { return false; }

		[[nodiscard]] virtual IdentifierExpression* GetIdentifier() noexcept { return nullptr; }

	};

	struct ExpressionStatement : Statement {
		ExpressionStatement(std::unique_ptr<Expression>&& expr, CodePosition cp) 
			: Statement(cp), m_pExpression(std::forward<decltype(expr)>(expr)){}

		void Resolve(TResolver& resolver) override {
			return m_pExpression->Resolve(resolver);
		}
		virtual void EmitByteCode(TBCBuilder& builder) override {
			return m_pExpression->EmitByteCode(builder, false);
		}

		std::unique_ptr<Expression> m_pExpression;
	};

	struct Program : BlockStatement {
		Program(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		bloop::BloopIndex m_uNumFunctions{};
		bloop::BloopIndex m_uNumGlobals{}; //includes functions

		//used by the bytecode generator
		std::vector<bloop::ast::FunctionDeclarationStatement*> m_oAllFunctions;
		std::vector<bloop::ast::FunctionDeclarationStatement*> m_oTopLevelFunctions;
	};

	struct LiteralExpression : Expression {
		LiteralExpression(const bloop::CodePosition& cp) : Expression(cp) {}
		[[nodiscard]] constexpr bool IsConst() const noexcept override { return true; }

		void Resolve([[maybe_unused]]TResolver& resolver) override {
			return; // do nothing
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			const auto idx = builder.AddConstant(m_oConstant);
			Emit(builder, TOpCode::LOAD_CONST, idx);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		};

		bloop::ConstantData m_oConstant;
	};

	struct IdentifierExpression : Expression {
		IdentifierExpression(const bloop::CodePosition& cp) : Expression(cp) {}
		[[nodiscard]] IdentifierExpression* GetIdentifier() noexcept override { return this; }
		using ResolvedIdentifier = bloop::resolver::internal::ResolvedIdentifier;
		void Resolve(TResolver& resolver) override {
			m_oResolver = resolver.ResolveIdentifier(m_sName);

			if(m_oResolver.m_eKind == ResolvedIdentifier::Kind::Error)
				throw bloop::exception::ResolverError(BLOOPTEXT("unknown identifier: ") + m_sName, m_oApproximatePosition);

			m_bIsConst = m_oResolver.m_bConst;
		}
		void Store(TBCBuilder& builder, bool want_value) {
			switch (m_oResolver.m_eKind) {
			case ResolvedIdentifier::Kind::Local:
				Emit(builder, TOpCode::STORE_LOCAL, m_oResolver.m_uSlot);
				Emit(builder, TOpCode::LOAD_LOCAL, m_oResolver.m_uSlot);
				break;
			case ResolvedIdentifier::Kind::Upvalue:
				Emit(builder, TOpCode::STORE_UPVALUE, m_oResolver.m_uSlot);
				Emit(builder, TOpCode::LOAD_UPVALUE, m_oResolver.m_uSlot);
				break;
			case ResolvedIdentifier::Kind::Global:
				Emit(builder, TOpCode::STORE_GLOBAL, m_oResolver.m_uSlot);
				Emit(builder, TOpCode::LOAD_GLOBAL, m_oResolver.m_uSlot);
				break;
			}

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			switch (m_oResolver.m_eKind) {
			case ResolvedIdentifier::Kind::Local:
				Emit(builder, TOpCode::LOAD_LOCAL, m_oResolver.m_uSlot);
				break;
			case ResolvedIdentifier::Kind::Upvalue:
				Emit(builder, TOpCode::LOAD_UPVALUE, m_oResolver.m_uSlot);
				break;
			case ResolvedIdentifier::Kind::Global:
				Emit(builder, TOpCode::LOAD_GLOBAL, m_oResolver.m_uSlot);
				break;
			}

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}
		[[nodiscard]] constexpr bool IsConst() const noexcept override { return m_bIsConst; }

		bloop::BloopString m_sName;
		bloop::BloopBool m_bIsConst{};
		bloop::resolver::internal::ResolvedIdentifier m_oResolver{};
	};

	struct BinaryExpression : Expression {
		BinaryExpression(const bloop::CodePosition& cp) : Expression(cp) {}
		BinaryExpression(bloop::EPunctuation punc, const bloop::CodePosition& cp) : Expression(cp), m_ePunctuation(punc){}

		virtual void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
			right->Resolve(resolver);
		}

		virtual void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			left->EmitByteCode(builder, true);
			right->EmitByteCode(builder, true);

			if (!bloop::bytecode::conversionTable.contains(m_ePunctuation))
				throw bloop::exception::ByteCodeError(BLOOPTEXT("unsupported operation"), m_oApproximatePosition);

			Emit(builder, bloop::bytecode::conversionTable[m_ePunctuation]);

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		bloop::EPunctuation m_ePunctuation{};
		std::unique_ptr<Expression> left;
		std::unique_ptr<Expression> right;
	};
	struct AssignExpression : BinaryExpression {
		AssignExpression(const bloop::CodePosition& cp) 
			: BinaryExpression(bloop::EPunctuation::p_assign, cp) {}

		void Resolve(TResolver& resolver) override;
		void EmitByteCode(TBCBuilder& builder, bool want_value) override;
	};
	struct FunctionExpression : Expression {

		FunctionExpression(std::unique_ptr<FunctionDeclarationStatement>&& funcDecl, const bloop::CodePosition& cp);
		~FunctionExpression();

		void Resolve(TResolver& resolver) override;
		void EmitByteCode(TBCBuilder& builder, bool want_value) override;

		std::unique_ptr<FunctionDeclarationStatement> m_pFuncDecl;
	};
	struct ArrayExpression : Expression {

		ArrayExpression(std::vector<std::unique_ptr<Expression>>&& inits, const bloop::CodePosition& cp) 
			: Expression(cp), m_pInitializers(std::forward<decltype(inits)>(inits)) {}

		void Resolve(TResolver& resolver) override {
			for (auto& v : m_pInitializers)
				v->Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			for (auto& v : m_pInitializers)
				v->EmitByteCode(builder, true);

			Emit(builder, TOpCode::CREATE_ARRAY, static_cast<bloop::BloopIndex>(m_pInitializers.size()));

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		std::vector<std::unique_ptr<Expression>> m_pInitializers;
	};

	struct ObjectExpression : Expression {

		struct KV {
			bloop::ConstantData key;
			std::unique_ptr<Expression> value;
		};
		
		ObjectExpression(const bloop::CodePosition& cp)
			: Expression(cp) {}

		void Resolve(TResolver& resolver) override {

			for (auto& kv : m_oKeyValues) {
				assert(kv && kv->value);
				kv->value->Resolve(resolver);
			}

		}

		void EmitByteCode(TBCBuilder& builder, bool want_value) override {
			for (const auto& kv : m_oKeyValues) {
				const auto idx = builder.AddConstant(kv->key);
				Emit(builder, TOpCode::LOAD_CONST, idx); // key
				kv->value->EmitByteCode(builder, true); // value
			}

			Emit(builder, TOpCode::CREATE_OBJECT, static_cast<bloop::BloopIndex>(m_oKeyValues.size()));

			if (!want_value)
				Emit(builder, TOpCode::POP);
		}

		std::vector<std::unique_ptr<KV>> m_oKeyValues;
	};

	struct VariableDeclaration : Statement {
		VariableDeclaration(const bloop::BloopString& name, std::unique_ptr<ExpressionStatement>&& init, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_pExpression(std::forward<decltype(init)>(init)) {}

		void Resolve(TResolver& resolver) override {
			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(bloop::fmt::format(BLOOPTEXT("identifier \"{}\" already declared"), m_sName), m_oApproximatePosition);
			}
			
			//prevent a = a by doing this after -> or not lol
			auto symbol = resolver.DeclareSymbol(m_sName, false);
			m_uSlot = symbol->m_uSlot;

			if(m_pExpression)
				m_pExpression->Resolve(resolver);

			symbol->m_bIsConst = IsConst();
		}

		void EmitByteCode(TBCBuilder& builder) override {
			if (m_pExpression)
				m_pExpression->EmitByteCode(builder);
		}

		[[nodiscard]] virtual constexpr bool IsConst() const noexcept { return false; }
		[[nodiscard]] constexpr bool IsDeclaration() const noexcept override { return true; }

		bloop::BloopString m_sName;
		std::unique_ptr<ExpressionStatement> m_pExpression;
		bloop::BloopIndex m_uSlot{ bloop::INVALID_SLOT };
	};

	struct ConstVariableDeclaration : VariableDeclaration {
		ConstVariableDeclaration(const bloop::BloopString& name, std::unique_ptr<ExpressionStatement>&& init, const bloop::CodePosition& cp)
			: VariableDeclaration(name, std::forward<decltype(init)>(init), cp){}

		[[nodiscard]] constexpr bool IsConst() const noexcept override { return true; }
	};

}