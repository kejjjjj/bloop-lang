#pragma once

#include "ast/ast.hpp"

namespace bloop::ast {

	struct WhileStatement : BlockStatement {
		WhileStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		void Resolve(TResolver& resolver) override {
			m_pCondition->Resolve(resolver);

			resolver.m_iLoopDepth++;
			BlockStatement::Resolve(resolver);
			resolver.m_iLoopDepth--;
		}

		void EmitByteCode(TBCBuilder& builder) override {

			const auto loopStart = builder.m_uOffset;
			m_pCondition->EmitByteCode(builder);

			const auto jumpExit = EmitJump(builder, TOpCode::JZ); //get the beginning of the loop
			builder.m_oLoops.push_back({});
			BlockStatement::EmitByteCode(builder);

			EmitJump(builder, TOpCode::JMP, loopStart); //jump back to the beginning of the loop
			PatchJump(builder, jumpExit, builder.m_uOffset); //patch the JZ statement to jump past the end of the loop

			for (const auto& _break : builder.m_oLoops.back().m_oBreakStatements)
				PatchJump(builder, _break, builder.m_uOffset); // make every break statement jump to the end

			for (const auto& _continue : builder.m_oLoops.back().m_oContinueStatements)
				PatchJump(builder, _continue, loopStart); // make every continue statement jump to the beginning

			builder.m_oLoops.pop_back();
		}

		std::unique_ptr<Expression> m_pCondition;
	};

	struct IfStatement : Statement {

		struct Structure {
			std::unique_ptr<Expression> m_pCondition;
			std::unique_ptr<BlockStatement> m_pBody;
		};

		IfStatement(const CodePosition& cp) : Statement(cp) {}

		void Resolve(TResolver& resolver) override {
			std::ranges::for_each(m_oIf, [&resolver](std::unique_ptr<Structure>& v) -> void {
				assert(v->m_pCondition && v->m_pBody);
				v->m_pCondition->Resolve(resolver);
				v->m_pBody->Resolve(resolver);
				});

			if (m_pElse)
				m_pElse->Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {

			std::vector<bloop::BloopIndex> m_oBlockEndJmps;
			std::optional<bloop::BloopIndex> nextJump;

			const auto jumpRequired = m_oIf.size() > 1u || m_pElse;

			for (auto& block : m_oIf) {

				if (nextJump) { //previous JZ jumps here
					PatchJump(builder, *nextJump, builder.m_uOffset);
					nextJump = std::nullopt;
				}
				block->m_pCondition->EmitByteCode(builder);
				nextJump = EmitJump(builder, TOpCode::JZ);
				block->m_pBody->EmitByteCode(builder);

				assert(!block->m_pBody->m_oStatements.empty());
				const auto lastIsReturn = block->m_pBody->m_oStatements.back()->IsReturn();

				if (jumpRequired && !lastIsReturn)
					m_oBlockEndJmps.push_back(EmitJump(builder, TOpCode::JMP)); //each block must jump to the end of this chain
			}

			if (nextJump) { //previous JZ jumps here
				PatchJump(builder, *nextJump, builder.m_uOffset);
				nextJump = std::nullopt;
			}

			if (m_pElse) {
				m_pElse->EmitByteCode(builder);
			}

			//make every block jump here after they're done
			for (const auto jmp : m_oBlockEndJmps)
				PatchJump(builder, jmp, builder.m_uOffset);


		}
		std::vector<std::unique_ptr<Structure>> m_oIf;
		std::unique_ptr<BlockStatement> m_pElse;

	};

	struct ForStatement : BlockStatement {
		[[nodiscard]] constexpr bool IsReturn() const noexcept override { return true; }

		ForStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		void Resolve(TResolver& resolver) override {

			resolver.BeginScope();

			if (m_pInitializer)
				m_pInitializer->Resolve(resolver);

			if (m_pCondition)
				m_pCondition->Resolve(resolver);

			resolver.m_iLoopDepth++;
			BlockStatement::ResolveNoScopeManagement(resolver);

			if (m_pOnEnd)
				m_pOnEnd->Resolve(resolver);

			resolver.m_iLoopDepth--;
			resolver.EndScope();
		}

		void EmitByteCode(TBCBuilder& builder) override {

			if (m_pInitializer)
				m_pInitializer->EmitByteCode(builder);

			const auto loopStart = builder.m_uOffset;

			if (m_pCondition)
				m_pCondition->EmitByteCode(builder);

			const auto jumpExit = EmitJump(builder, TOpCode::JZ); //get the beginning of the loop
			builder.m_oLoops.push_back({});
			BlockStatement::EmitByteCode(builder);

			const auto onEndPos = builder.m_uOffset;

			if (m_pOnEnd)
				m_pOnEnd->EmitByteCode(builder);

			EmitJump(builder, TOpCode::JMP, loopStart); //jump back to the beginning of the loop
			PatchJump(builder, jumpExit, builder.m_uOffset); //patch the JZ statement to jump past the end of the loop

			for (const auto& _break : builder.m_oLoops.back().m_oBreakStatements)
				PatchJump(builder, _break, builder.m_uOffset); // make every break statement jump to the end

			for (const auto& _continue : builder.m_oLoops.back().m_oContinueStatements)
				PatchJump(builder, _continue, onEndPos); // make every continue statement jump to the end expression

			builder.m_oLoops.pop_back();
		}

		std::unique_ptr<Statement> m_pInitializer;
		std::unique_ptr<Expression> m_pCondition;
		std::unique_ptr<Expression> m_pOnEnd;
	};

	struct ReturnStatement : ExpressionStatement {
		[[nodiscard]] constexpr bool IsReturn() const noexcept override { return true; }

		ReturnStatement(std::unique_ptr<Expression>&& expr, const bloop::CodePosition& cp) :
			ExpressionStatement(std::forward<decltype(expr)>(expr), cp) {
		}

		void Resolve(TResolver& resolver) override {

			if (resolver.m_oFunctions.empty())
				throw bloop::exception::ResolverError(BLOOPTEXT("don't return in the global scope"), m_oApproximatePosition);

			if (m_pExpression)
				ExpressionStatement::Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			if (m_pExpression) {
				m_pExpression->EmitByteCode(builder);
				Emit(builder, TOpCode::RETURN_VALUE);
				return;
			}
			Emit(builder, TOpCode::RETURN);
		}

	};

	struct ContinueStatement : Statement {
		ContinueStatement(const bloop::CodePosition& cp) : Statement(cp) {}

		void Resolve(TResolver& resolver) override {
			if (!resolver.m_iLoopDepth)
				throw exception::ResolverError(BLOOPTEXT("continue statement must be in a loop context"), m_oApproximatePosition);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			assert(builder.m_oLoops.empty() == false);
			builder.m_oLoops.back().m_oContinueStatements.push_back(EmitJump(builder, TOpCode::JMP));
		}
	};

	struct BreakStatement : Statement {
		BreakStatement(const bloop::CodePosition& cp) : Statement(cp) {}

		void Resolve(TResolver& resolver) override {
			if (!resolver.m_iLoopDepth)
				throw exception::ResolverError(BLOOPTEXT("break statement must be in a loop context"), m_oApproximatePosition);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			assert(builder.m_oLoops.empty() == false);
			builder.m_oLoops.back().m_oBreakStatements.push_back(EmitJump(builder, TOpCode::JMP));
		}
	};
}