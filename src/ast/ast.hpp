#pragma once

#include "utils/defs.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <vector>
#include <utility>

namespace bloop::ast {

	struct AbstractSyntaxTree	{
		virtual ~AbstractSyntaxTree() = default;

		[[nodiscard]] constexpr virtual bool IsRoot() const noexcept { return false; }
		[[nodiscard]] constexpr virtual bool IsStatement() const noexcept { return false; }
		[[nodiscard]] constexpr virtual bool IsExpression() const noexcept { return false; }

		bloop::CodePosition m_oApproximatePosition;
	};

	struct Statement : AbstractSyntaxTree {
		[[nodiscard]] constexpr bool IsStatement() const noexcept override { return true; }
	};

	struct BlockStatement : Statement {

		void AddStatement(std::unique_ptr<Statement>&& stmt) {
			m_oStatements.emplace_back(std::forward<decltype(stmt)>(stmt));
		}

		std::vector<std::unique_ptr<Statement>> m_oStatements;
	};

	struct Expression : AbstractSyntaxTree {
		[[nodiscard]] constexpr bool IsExpression() const noexcept override { return true; }
	};

	struct ExpressionStatement : Statement {
		ExpressionStatement(std::unique_ptr<Expression>&& expr) 
			: m_pExpression(std::forward<decltype(expr)>(expr)){}
		std::unique_ptr<Expression> m_pExpression;
	};

	struct Program : BlockStatement {
		[[nodiscard]] constexpr bool IsRoot() const noexcept override { return true; }
	};

	struct LiteralExpression : Expression {
		bloop::BloopString m_pConstant;
		bloop::EValueType m_eDataType{};
	};

	struct BinaryExpression : Expression {
		BinaryExpression(bloop::EPunctuation punc) : m_ePunctuation(punc){}

		bloop::EPunctuation m_ePunctuation{};
		std::unique_ptr<Expression> left;
		std::unique_ptr<Expression> right;
	};


}