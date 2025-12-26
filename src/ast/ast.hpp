#pragma once

#include "utils/defs.hpp"
#include "lexer/token.hpp"
#include "resolver/resolver.hpp"
#include "resolver/exception.hpp"
#include "bytecode/compile/emit.hpp"
#include "bytecode/exception.hpp"

#include <memory>
#include <vector>
#include <utility>


namespace bloop::ast {
	using TResolver = bloop::resolver::internal::Resolver;
	using TBCBuilder = bloop::bytecode::CByteCodeBuilder;
	using TOpCode = bloop::bytecode::EOpCode;

	struct AbstractSyntaxTree	{
		AbstractSyntaxTree(bloop::CodePosition cp) : m_oApproximatePosition(cp){}
		virtual ~AbstractSyntaxTree() = default;
		bloop::CodePosition m_oApproximatePosition;
	};

	struct Statement : AbstractSyntaxTree {
		Statement(bloop::CodePosition cp) : AbstractSyntaxTree(cp){}
		virtual void Resolve(TResolver& resolver) = 0;
		virtual void EmitByteCode(TBCBuilder& builder) = 0;
		[[nodiscard]] virtual constexpr bool IsFunction() const noexcept { return false; }

	};

	struct BlockStatement : Statement {
		BlockStatement(const bloop::CodePosition& cp) : Statement(cp){}
		virtual void Resolve(TResolver& resolver) override {
			resolver.BeginScope();
			std::ranges::for_each(m_oStatements, [&resolver](const auto& s) { s->Resolve(resolver); });
			resolver.EndScope();
		}

		void ResolveNoScopeManagement(TResolver& resolver) {
			std::ranges::for_each(m_oStatements, [&resolver](const auto& s) { s->Resolve(resolver); });
		}

		virtual void EmitByteCode(TBCBuilder& builder) override {
			std::ranges::for_each(m_oStatements, [&builder](const auto& s) { s->EmitByteCode(builder); });
		}

		void AddStatement(std::unique_ptr<Statement>&& stmt) {
			m_oStatements.emplace_back(std::forward<decltype(stmt)>(stmt));
		}
		
		std::vector<std::unique_ptr<Statement>> m_oStatements;
	};

	struct Expression : AbstractSyntaxTree {
		Expression(const bloop::CodePosition& cp) : AbstractSyntaxTree(cp) {}

		virtual void Resolve(TResolver& resolver) = 0;
		virtual void EmitByteCode(TBCBuilder& builder) = 0;
		[[nodiscard]] virtual constexpr bool IsConst() const noexcept { return false; }

	};

	struct ExpressionStatement : Statement {
		ExpressionStatement(std::unique_ptr<Expression>&& expr, CodePosition cp) 
			: Statement(cp), m_pExpression(std::forward<decltype(expr)>(expr)){}

		void Resolve(TResolver& resolver) override {
			return m_pExpression->Resolve(resolver);
		}
		virtual void EmitByteCode(TBCBuilder& builder) override {
			return m_pExpression->EmitByteCode(builder);
		}

		std::unique_ptr<Expression> m_pExpression;
	};

	struct Program : BlockStatement {
		Program(const bloop::CodePosition& cp) : BlockStatement(cp) {}
	};

	struct LiteralExpression : Expression {
		LiteralExpression(const bloop::CodePosition& cp) : Expression(cp) {}
		[[nodiscard]] constexpr bool IsConst() const noexcept override { return true; }

		void Resolve([[maybe_unused]]TResolver& resolver) override {
			return; // do nothing
		}
		void EmitByteCode(TBCBuilder& builder) override {
			const auto idx = builder.AddConstant(bloop::bytecode::CConstant{ .m_pConstant = m_pConstant, .m_eDataType = m_eDataType });
			builder.Emit(TOpCode::LOAD_CONST, idx);
		};

		bloop::BloopString m_pConstant;
		bloop::EValueType m_eDataType{};
	};

	struct IdentifierExpression : Expression {
		IdentifierExpression(const bloop::CodePosition& cp) : Expression(cp) {}

		void Resolve(TResolver& resolver) override {
			if (const auto symbol = resolver.ResolveSymbol(m_sName)) {
				m_iDepth = symbol->m_iDepth;
				m_uSlot = symbol->m_uSlot;
				m_bIsConst = symbol->m_bIsConst;
				return;
			}

			throw bloop::exception::ResolverError(BLOOPTEXT("unknown identifier: ") + m_sName, m_oApproximatePosition);

		}
		void EmitByteCode(TBCBuilder& builder) override {
			builder.Emit(TOpCode::LOAD_LOCAL, m_uSlot);
		}
		[[nodiscard]] constexpr bool IsConst() const noexcept override { return m_bIsConst; }

		bloop::BloopString m_sName;
		bloop::BloopInt m_iDepth{ bloop::bytecode::INVALID_SLOT };
		bloop::BloopUInt16 m_uSlot{ bloop::bytecode::INVALID_SLOT };
		bloop::BloopBool m_bIsConst{};
	};

	struct BinaryExpression : Expression {
		BinaryExpression(bloop::EPunctuation punc, const bloop::CodePosition& cp) : Expression(cp), m_ePunctuation(punc){}

		virtual void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
			right->Resolve(resolver);

		}

		virtual void EmitByteCode(TBCBuilder& builder) override {
			left->EmitByteCode(builder);
			right->EmitByteCode(builder);

			if (!bloop::bytecode::conversionTable.contains(m_ePunctuation))
				throw bloop::exception::ByteCodeError(BLOOPTEXT("unsupported operation"), m_oApproximatePosition);

			builder.Emit(bloop::bytecode::conversionTable[m_ePunctuation]);
		}

		bloop::EPunctuation m_ePunctuation{};
		std::unique_ptr<Expression> left;
		std::unique_ptr<Expression> right;
	};

	struct AssignExpression : BinaryExpression {
		AssignExpression(const bloop::CodePosition& cp) 
			: BinaryExpression(bloop::EPunctuation::p_assign, cp) {}

		void Resolve(TResolver& resolver) override {
			BinaryExpression::Resolve(resolver);

			if (left->IsConst())
				throw bloop::exception::ResolverError(BLOOPTEXT("lhs is declared as const"), left->m_oApproximatePosition);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			right->EmitByteCode(builder);

			if (const auto ptr = dynamic_cast<IdentifierExpression*>(left.get())) {
				builder.Emit(TOpCode::STORE_LOCAL, ptr->m_uSlot);
				return;
			}
			throw bloop::exception::ResolverError(BLOOPTEXT("lhs wasn't an identifier"), left->m_oApproximatePosition);

		}
	};

	struct FunctionDeclarationStatement : Statement {
		FunctionDeclarationStatement(const BloopString& name, std::vector<BloopString>&& params, 
			std::unique_ptr<BlockStatement>&& body, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_oParams(std::forward<decltype(params)>(params)), m_pBody(std::forward<decltype(body)>(body)){ }
		[[nodiscard]] constexpr bool IsFunction() const noexcept override { return true; }

		void Resolve(TResolver& resolver) override {

			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("function already declared: ") + m_sName, m_oApproximatePosition);
			}

			resolver.DeclareSymbol(m_sName, true);
			m_iFunctionId = resolver.m_uNumFunctions++;

			resolver.m_oFunctions.push_back({});
			resolver.BeginScope();

			std::ranges::for_each(m_oParams, [this, &resolver](const std::string& param) {
				if(resolver.ResolveSymbol(param))
					throw bloop::exception::ResolverError(BLOOPTEXT("variable already declared: ") + param, m_oApproximatePosition);
			});

			m_pBody->ResolveNoScopeManagement(resolver);
			m_uLocalCount = resolver.m_oFunctions.back().m_uNextSlot;
			resolver.EndScope();
			resolver.m_oFunctions.pop_back();
		}

		void EmitByteCode(TBCBuilder& builder) override {
			m_pBody->EmitByteCode(builder);
			assert(!builder.m_oByteCode.empty());
			if(builder.m_oByteCode.back().GetOpCode() != TOpCode::RETURN && builder.m_oByteCode.back().GetOpCode() != TOpCode::RETURN_VALUE)
				builder.Emit(TOpCode::RETURN);
		}

		bloop::BloopString m_sName;
		std::vector<BloopString> m_oParams;
		std::unique_ptr<BlockStatement> m_pBody;

		bloop::BloopUInt m_iFunctionId{0};
		bloop::BloopUInt16 m_uLocalCount{0};
	};

	struct VariableDeclaration : Statement {
		VariableDeclaration(const bloop::BloopString& name, std::unique_ptr<Expression>&& init, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_pInitializer(std::forward<decltype(init)>(init)) {}

		void Resolve(TResolver& resolver) override {
			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("variable already declared: ") + m_sName, m_oApproximatePosition);
			}
			
			if (m_pInitializer)
				m_pInitializer->Resolve(resolver);

			//prevent a = a by doing this after
			m_uSlot = resolver.DeclareSymbol(m_sName, IsConst())->m_uSlot;
		}

		void EmitByteCode(TBCBuilder& builder) override {
			if (m_pInitializer) {
				m_pInitializer->EmitByteCode(builder);
				builder.Emit(TOpCode::STORE_LOCAL, m_uSlot);
			}
		}

		[[nodiscard]] virtual constexpr bool IsConst() const noexcept { return false; }
		bloop::BloopString m_sName;
		std::unique_ptr<Expression> m_pInitializer;
		bloop::BloopUInt16 m_uSlot{ bloop::bytecode::INVALID_SLOT };
	};

	struct ConstVariableDeclaration : VariableDeclaration {
		ConstVariableDeclaration(const bloop::BloopString& name, std::unique_ptr<Expression>&& init, const bloop::CodePosition& cp)
			: VariableDeclaration(name, std::forward<decltype(init)>(init), cp){}

		[[nodiscard]] constexpr bool IsConst() const noexcept override { return true; }
	};

	struct WhileStatement : BlockStatement {
		WhileStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		void Resolve(TResolver& resolver) override {
			m_pCondition->Resolve(resolver);
			BlockStatement::Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {

			const auto loopStart = builder.m_uOffset;
			m_pCondition->EmitByteCode(builder);

			const auto jumpExit = builder.EmitJump(TOpCode::JZ); //get the beginning of the loop
			BlockStatement::EmitByteCode(builder);

			builder.EmitJump(TOpCode::JMP, loopStart); //jump back to the beginning of the loop
			builder.PatchJump(jumpExit, builder.m_uOffset); //patch the JZ statement to jump past the end of the loop
		}

		std::unique_ptr<Expression> m_pCondition;
	};

	struct ReturnStatement : ExpressionStatement {

		ReturnStatement(std::unique_ptr<Expression>&& expr, const bloop::CodePosition& cp) : 
			ExpressionStatement(std::forward<decltype(expr)>(expr), cp){}

		void Resolve(TResolver& resolver) override {
			if (m_pExpression)
				ExpressionStatement::Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			if (m_pExpression) {
				m_pExpression->EmitByteCode(builder);
				builder.Emit(TOpCode::RETURN_VALUE);
				return;
			}
			builder.Emit(TOpCode::RETURN);
		}

	};

}