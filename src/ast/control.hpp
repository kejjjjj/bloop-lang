#pragma once

#include "ast/ast.hpp"
#include "bytecode/exception.hpp"

#include <optional>
#include <vector>

namespace bloop::ast {

	// emits the value that an if/while condition tests (it stays on the stack for JZ).
	// the condition is an expression, or a declaration with an initializer: if (const x = f())
	inline void EmitCondition(TBCBuilder& builder, Statement& condition) {
		if (auto* expression = dynamic_cast<ExpressionStatement*>(&condition)) {
			expression->m_pExpression->EmitByteCode(builder, true);
			return;
		}
		if (auto* declaration = dynamic_cast<VariableDeclaration*>(&condition); declaration && declaration->m_pExpression) {
			declaration->m_pExpression->m_pExpression->EmitByteCode(builder, true); // the assignment, its value is the assigned value
			return;
		}
		throw bloop::exception::ByteCodeError(BLOOPTEXT("the condition doesn't have a value"), condition.m_oApproximatePosition);
	}


	struct WhileStatement : BlockStatement {
		WhileStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		void Resolve(TResolver& resolver) override {
			resolver.BeginScope();
			m_pCondition->Resolve(resolver);

			resolver.m_iLoopDepth++;
			BlockStatement::ResolveNoScopeManagement(resolver);
			resolver.m_iLoopDepth--;
			resolver.EndScope();
		}

		void EmitByteCode(TBCBuilder& builder) override {

			const auto loopStart = builder.m_uOffset;
			EmitCondition(builder, *m_pCondition);

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

		std::unique_ptr<Statement> m_pCondition;
	};

	struct IfStatement : Statement {

		struct Structure {
			std::unique_ptr<Statement> m_pCondition;
			std::unique_ptr<BlockStatement> m_pBody;
		};

		IfStatement(const CodePosition& cp) : Statement(cp) {}

		void Resolve(TResolver& resolver) override {
			std::ranges::for_each(m_oIf, [&resolver](std::unique_ptr<Structure>& v) -> void {
				assert(v->m_pCondition && v->m_pBody);
				resolver.BeginScope();
				v->m_pCondition->Resolve(resolver);
				v->m_pBody->ResolveNoScopeManagement(resolver);
				resolver.EndScope();
			});

			if (m_pElse)
				m_pElse->Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {

			std::vector<bloop::BloopOffset> m_oBlockEndJmps;
			std::optional<bloop::BloopOffset> nextJump;

			const auto jumpRequired = m_oIf.size() > 1u || m_pElse;

			for (auto& block : m_oIf) {

				if (nextJump) { //previous JZ jumps here
					PatchJump(builder, *nextJump, builder.m_uOffset);
					nextJump = std::nullopt;
				}
				EmitCondition(builder, *block->m_pCondition);

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

			// for (;;) has no condition: nothing to test, only break leaves the loop
			std::optional<bloop::BloopOffset> jumpExit;
			if (m_pCondition) {
				m_pCondition->EmitByteCode(builder, true);
				jumpExit = EmitJump(builder, TOpCode::JZ);
			}
			builder.m_oLoops.push_back({});
			BlockStatement::EmitByteCode(builder);

			const auto onEndPos = builder.m_uOffset;

			if (m_pOnEnd)
				m_pOnEnd->EmitByteCode(builder);

			EmitJump(builder, TOpCode::JMP, loopStart); //jump back to the beginning of the loop
			if (jumpExit)
				PatchJump(builder, *jumpExit, builder.m_uOffset); //patch the JZ statement to jump past the end of the loop

			for (const auto& _break : builder.m_oLoops.back().m_oBreakStatements)
				PatchJump(builder, _break, builder.m_uOffset); // make every break statement jump to the end

			for (const auto& _continue : builder.m_oLoops.back().m_oContinueStatements)
				PatchJump(builder, _continue, onEndPos); // make every continue statement jump to the end expression

			builder.m_oLoops.pop_back();
		}

		std::unique_ptr<Statement> m_pInitializer;
		std::unique_ptr<Expression> m_pCondition;
		std::unique_ptr<ExpressionStatement> m_pOnEnd;
	};

	// for (const element : iterable) { ... }
	// Like js' for...of: the iterable is evaluated once, every iteration gets its own (const) binding of the element,
	// and the element is a copy of the array element (objects are still shared). Arrays and strings are iterable.
	struct RangedForStatement : BlockStatement {
		RangedForStatement(const bloop::CodePosition& cp) : BlockStatement(cp) {}

		void Resolve(TResolver& resolver) override {

			resolver.BeginScope();

			// before the element is declared: for (const x : x) refers to an outer x
			m_pIterable->Resolve(resolver);

			if (resolver.ResolveSymbol(m_sName))
				throw bloop::exception::ResolverError(bloop::fmt::format(BLOOPTEXT("identifier \"{}\" already declared"), m_sName), m_oApproximatePosition);

			auto* symbol = resolver.DeclareSymbol(m_sName, false);
			symbol->m_bIsConst = m_bIsConst;

			// at the top level the element is a global, which a closure would read when it's called (after the loop
			// has moved on). Capturing its value when the closure is created gives every iteration its own binding
			symbol->m_bCapturedByValue = resolver.m_oFunctions.empty();
			m_oElement = resolver.ResolveIdentifier(m_sName);
			assert(m_oElement.m_eKind == ResolvedIdentifier::Kind::Local || m_oElement.m_eKind == ResolvedIdentifier::Kind::Global);

			resolver.m_iLoopDepth++;
			BlockStatement::ResolveNoScopeManagement(resolver);
			resolver.m_iLoopDepth--;

			resolver.EndScope();
		}

		void EmitByteCode(TBCBuilder& builder) override {

			// the iterable and the index stay on the stack for the whole loop
			m_pIterable->EmitByteCode(builder, true);
			Emit(builder, TOpCode::ITER_PREPARE);

			const auto loopStart = builder.m_uOffset;
			const auto jumpExit = EmitJump(builder, TOpCode::ITER_NEXT);

			const auto isLocal = m_oElement.m_eKind == ResolvedIdentifier::Kind::Local;
			Emit(builder, isLocal ? TOpCode::STORE_LOCAL : TOpCode::STORE_GLOBAL, m_oElement.m_uSlot);

			builder.m_oLoops.push_back({});
			BlockStatement::EmitByteCode(builder);

			const auto continueTarget = builder.m_uOffset;

			// a fresh binding for every iteration: closures created in the body keep this iteration's values
			if (isLocal)
				Emit(builder, TOpCode::CLOSE_UPVALUES, m_oElement.m_uSlot);

			EmitJump(builder, TOpCode::JMP, loopStart);

			const auto exit = builder.m_uOffset;
			PatchJump(builder, jumpExit, exit);

			if (isLocal)
				Emit(builder, TOpCode::CLOSE_UPVALUES, m_oElement.m_uSlot); // after a break

			Emit(builder, TOpCode::POP); // index
			Emit(builder, TOpCode::POP); // iterable

			for (const auto& _break : builder.m_oLoops.back().m_oBreakStatements)
				PatchJump(builder, _break, exit);

			for (const auto& _continue : builder.m_oLoops.back().m_oContinueStatements)
				PatchJump(builder, _continue, continueTarget);

			builder.m_oLoops.pop_back();
		}

		using ResolvedIdentifier = bloop::resolver::internal::ResolvedIdentifier;

		bloop::BloopString m_sName;
		bloop::BloopBool m_bIsConst{};
		std::unique_ptr<Expression> m_pIterable;
		ResolvedIdentifier m_oElement{};
	};

	struct ReturnStatement : ExpressionStatement {
		[[nodiscard]] constexpr bool IsReturn() const noexcept override { return true; }

		ReturnStatement(std::unique_ptr<Expression>&& expr, const bloop::CodePosition& cp) :
			ExpressionStatement(std::forward<decltype(expr)>(expr), cp) {
		}

		void Resolve(TResolver& resolver) override {

			// all modules run as one piece of code, so returning from the top level of an imported
			// module would end the whole program
			if (resolver.m_oFunctions.empty() && resolver.m_bInImportedModule)
				throw bloop::exception::ResolverError(BLOOPTEXT("an imported module can't return from its global scope"), m_oApproximatePosition);

			if (m_pExpression)
				ExpressionStatement::Resolve(resolver);
		}

		void EmitByteCode(TBCBuilder& builder) override {
			if (m_pExpression) {
				m_pExpression->EmitByteCode(builder, true);
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