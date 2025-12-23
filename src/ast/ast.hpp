#pragma once

#include "utils/defs.hpp"
#include "lexer/token.hpp"
#include "resolver/resolver.hpp"
#include "resolver/exception.hpp"

#include <memory>
#include <vector>
#include <utility>


namespace bloop::ast {
	using TResolver = bloop::resolver::internal::Resolver;

	struct AbstractSyntaxTree	{
		AbstractSyntaxTree(bloop::CodePosition cp) : m_oApproximatePosition(cp){}
		virtual ~AbstractSyntaxTree() = default;
		bloop::CodePosition m_oApproximatePosition;
	};

	struct Statement : AbstractSyntaxTree {
		Statement(bloop::CodePosition cp) : AbstractSyntaxTree(cp){}
		virtual void Resolve(TResolver& resolver) = 0;
	};

	struct BlockStatement : Statement {
		BlockStatement(const bloop::CodePosition& cp) : Statement(cp){}
		void Resolve(TResolver& resolver) override {
			resolver.BeginScope();
			std::ranges::for_each(m_oStatements, [&resolver](const auto& s) { s->Resolve(resolver); });
			resolver.EndScope();
		}

		void ResolveNoScopeManagement(TResolver& resolver) {
			std::ranges::for_each(m_oStatements, [&resolver](const auto& s) { s->Resolve(resolver); });
		}

		void AddStatement(std::unique_ptr<Statement>&& stmt) {
			m_oStatements.emplace_back(std::forward<decltype(stmt)>(stmt));
		}

		std::vector<std::unique_ptr<Statement>> m_oStatements;
	};

	struct Expression : AbstractSyntaxTree {
		Expression(const bloop::CodePosition& cp) : AbstractSyntaxTree(cp) {}

		virtual void Resolve(TResolver& resolver) = 0;
	};

	struct ExpressionStatement : Statement {
		ExpressionStatement(std::unique_ptr<Expression>&& expr, CodePosition cp) 
			: Statement(cp), m_pExpression(std::forward<decltype(expr)>(expr)){}

		void Resolve(TResolver& resolver) override {
			return m_pExpression->Resolve(resolver);
		}

		std::unique_ptr<Expression> m_pExpression;
	};

	struct Program : BlockStatement {
		Program(const bloop::CodePosition& cp) : BlockStatement(cp) {}
	};

	struct LiteralExpression : Expression {
		LiteralExpression(const bloop::CodePosition& cp) : Expression(cp) {}

		void Resolve([[maybe_unused]]TResolver& resolver) override {
			return; // do nothing
		}

		bloop::BloopString m_pConstant;
		bloop::EValueType m_eDataType{};
	};

	struct IdentifierExpression : Expression {
		IdentifierExpression(const bloop::CodePosition& cp) : Expression(cp) {}

		void Resolve(TResolver& resolver) override {
			if (const auto symbol = resolver.ResolveSymbol(m_sName)) {
				m_iDepth = symbol->m_iDepth;
				m_iSlot = symbol->m_iSlot;
			}

			throw bloop::exception::ResolverError(BLOOPTEXT("unknown identifier: ") + m_sName, m_oApproximatePosition);

		}

		bloop::BloopString m_sName;
		bloop::BloopInt m_iDepth = -1;
		bloop::BloopInt m_iSlot = -1;

	};

	struct BinaryExpression : Expression {
		BinaryExpression(bloop::EPunctuation punc, const bloop::CodePosition& cp) : Expression(cp), m_ePunctuation(punc){}

		void Resolve(TResolver& resolver) override {
			left->Resolve(resolver);
			right->Resolve(resolver);
		}

		bloop::EPunctuation m_ePunctuation{};
		std::unique_ptr<Expression> left;
		std::unique_ptr<Expression> right;
	};

	struct FunctionDeclarationStatement : Statement {
		FunctionDeclarationStatement(const BloopString& name, std::vector<BloopString>&& params, 
			std::unique_ptr<BlockStatement>&& body, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_oParams(std::forward<decltype(params)>(params)), m_pBody(std::forward<decltype(body)>(body)){ }

		void Resolve(TResolver& resolver) override {

			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("function already declared: ") + m_sName, m_oApproximatePosition);
			}
			resolver.DeclareSymbol(m_sName);
			m_iFunctionId = resolver.m_uNumFunctions++;

			resolver.BeginScope();

			std::ranges::for_each(m_oParams, [this, &resolver](const std::string& param) {
				if(resolver.ResolveSymbol(param))
					throw bloop::exception::ResolverError(BLOOPTEXT("variable already declared: ") + param, m_oApproximatePosition);
			});

			m_pBody->ResolveNoScopeManagement(resolver);
			m_uLocalCount = resolver.m_oScopes.back().symbols.size();
			resolver.EndScope();
		}

		bloop::BloopString m_sName;
		std::vector<BloopString> m_oParams;
		std::unique_ptr<BlockStatement> m_pBody;

		bloop::BloopInt m_iFunctionId = -1;
		bloop::BloopUInt m_uLocalCount = 0;
	};

	struct VariableDeclaration : Statement {
		VariableDeclaration(const bloop::BloopString& name, std::unique_ptr<Expression>&& init, const bloop::CodePosition& cp)
			: Statement(cp), m_sName(name), m_pInitializer(std::forward<decltype(init)>(init)) {}

		void Resolve(TResolver& resolver) override {
			if (resolver.ResolveSymbol(m_sName)) {
				throw bloop::exception::ResolverError(BLOOPTEXT("variable already declared: ") + m_sName, m_oApproximatePosition);
			}
			
			if (m_pInitializer)
				return m_pInitializer->Resolve(resolver);

			//prevent a = a by doing this after
			m_iSlot = resolver.DeclareSymbol(m_sName)->m_iSlot;
		}

		[[nodiscard]] virtual constexpr bool IsConst() const noexcept { return false; }
		bloop::BloopString m_sName;
		std::unique_ptr<Expression> m_pInitializer;
		bloop::BloopInt m_iSlot = -1;
	};

	struct ConstVariableDeclaration : VariableDeclaration {
		ConstVariableDeclaration(const bloop::BloopString& name, std::unique_ptr<Expression>&& init, const bloop::CodePosition& cp)
			: VariableDeclaration(name, std::forward<decltype(init)>(init), cp){}

		[[nodiscard]] constexpr bool IsConst() const noexcept override { return true; }
	};

}