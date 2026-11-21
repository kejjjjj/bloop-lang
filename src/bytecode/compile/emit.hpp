#pragma once
#include "utils/defs.hpp"
#include "bytecode/defs.hpp"
#include "metadata/metadata.hpp"

#include <vector>
#include <variant>
#include <type_traits>
#include <string>

namespace bloop::ast {
	struct AbstractSyntaxTree;
}

namespace bloop::bytecode
{

	struct Instr0 {
		EOpCode op;
	};
	struct Instr1 {
		EOpCode op;
		bloop::BloopIndex arg;
	};
	struct Instr2 {
		EOpCode op;
		bloop::BloopIndex arg;
		bloop::BloopIndex arg2;
	};
	struct Instr3 {
		EOpCode op;
		bloop::BloopIndex arg;
		bloop::BloopIndex arg2;
		bloop::BloopIndex arg3;
	};
	// JMP, JZ, ITER_NEXT
	struct InstrJump {
		EOpCode op;
		bloop::BloopOffset target;
	};
	// TRY: where the catch block starts, the catch variable
	struct InstrJumpArg {
		EOpCode op;
		bloop::BloopOffset target;
		bloop::BloopIndex arg;
	};
	using Instruction = std::variant<Instr0, Instr1, Instr2, Instr3, InstrJump, InstrJumpArg>;

	struct CSingularByteCode {		
		Instruction ins;
		bc::InstrDebugRef loc; // for runtime error messages

		template<typename Instr>
		[[nodiscard]] static constexpr bloop::BloopOffset SizeOf() noexcept {
			bloop::BloopOffset result = 1u;
			if constexpr (requires(Instr i) { i.target; }) result += sizeof(bloop::BloopOffset);
			if constexpr (requires(Instr i) { i.arg; })    result += sizeof(bloop::BloopIndex);
			if constexpr (requires(Instr i) { i.arg2; })   result += sizeof(bloop::BloopIndex);
			if constexpr (requires(Instr i) { i.arg3; })   result += sizeof(bloop::BloopIndex);
			return result;
		}

		[[nodiscard]] bloop::BloopOffset GetBytes() const {
			return std::visit([](auto&& i) { return SizeOf<std::decay_t<decltype(i)>>(); }, ins);
		}

		[[nodiscard]] EOpCode GetOpCode() {
			EOpCode result{};
			std::visit([&](auto&& i) {
				result = i.op;
			}, ins);
			return result;
		}

		[[nodiscard]] bloop::BloopString ToString() const {
			bloop::BloopString res;
			std::visit([&](auto&& i) {
				res = stringConversionTable.at(i.op);

				if constexpr (std::is_same_v<std::decay_t<decltype(i)>, InstrJump>) {
					res += BLOOPTEXT(", -> ") + std::to_string(i.target);
				} else if constexpr (std::is_same_v<std::decay_t<decltype(i)>, InstrJumpArg>) {
					res += BLOOPTEXT(", -> ") + std::to_string(i.target) + BLOOPTEXT(" ") + std::to_string(i.arg);
				} else if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr1>) {
					res += BLOOPTEXT(", ") + std::to_string(i.arg);
				} else if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr2>) {
					res += BLOOPTEXT(", ") + std::to_string(i.arg) 
						+ BLOOPTEXT(" ") + std::to_string(i.arg2);
				} else if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr3>) {
					res += BLOOPTEXT(", ") + std::to_string(i.arg) 
						+ BLOOPTEXT(" ") + std::to_string(i.arg2) 
						+ BLOOPTEXT(" ") + std::to_string(i.arg3);
				}

			}, ins);

			return res;
		}
	};

	//contains the instruction offsets
	struct LoopContext {
		std::vector<bloop::BloopOffset> m_oBreakStatements;
		std::vector<bloop::BloopOffset> m_oContinueStatements;
	};

	struct CByteCodeBuilder {
		CByteCodeBuilder(bloop::metadata::Metadata& metadata) : m_refMetadata(metadata){}

		virtual ~CByteCodeBuilder() = default;
		[[nodiscard]] virtual constexpr bool GlobalContext() const noexcept { return false; }
		[[nodiscard]] bloop::BloopIndex AddConstant(bloop::ConstantData c);

		void Emit(EOpCode opcode, bloop::BloopIndex arg, bloop::BloopIndex arg2, bloop::BloopIndex arg3, CodePosition pos);
		void Emit(EOpCode opcode, bloop::BloopIndex arg, bloop::BloopIndex arg2, CodePosition pos);
		void Emit(EOpCode opcode, bloop::BloopIndex idx, CodePosition pos);
		void Emit(EOpCode opcode, CodePosition pos);

		// jumps: a 32-bit target (a byte offset in this chunk)
		[[nodiscard]] bloop::BloopOffset EmitJump(EOpCode opcode, CodePosition pos); //returns the index of the instruction in m_oByteCode
		void EmitJump(EOpCode opcode, bloop::BloopOffset target, CodePosition pos);
		void PatchJump(bloop::BloopOffset src, bloop::BloopOffset dst);

		[[nodiscard]] bloop::BloopOffset EmitTry(EOpCode opcode, bloop::BloopIndex catchVar, CodePosition pos); //returns the index of the instruction in m_oByteCode
		void PatchTry(bloop::BloopOffset src, bloop::BloopOffset dst);


		void EmitCapture(const bc::Capture& capture, CodePosition pos);
		void EnsureReturn(bloop::ast::AbstractSyntaxTree* node);
		void AddFunction(bloop::BloopIndex idx);
		[[nodiscard]] inline auto FunctionCount() const noexcept { return m_oFunctions.size(); }
		[[nodiscard]] bc::Chunk Finalize();

		void Print();

		[[nodiscard]] std::vector<bloop::BloopByte> Encode();
		[[nodiscard]] std::vector<bc::InstrDebugRef> GetCodePositions();

		std::vector<bloop::ConstantData> m_oConstants;
		std::vector<CSingularByteCode> m_oByteCode;
		bloop::BloopOffset m_uOffset{}; // the size of the bytecode so far
		bloop::BloopBool m_bIsLambda{};

		std::vector<LoopContext> m_oLoops;

		bloop::metadata::Metadata& m_refMetadata;

	private:
		void Add(Instruction ins, CodePosition pos);

		std::vector<bloop::BloopIndex> m_oFunctions; // references metadata
	};

	//struct CByteCodeBuilderForGlobals : CByteCodeBuilder {
	//	CByteCodeBuilderForGlobals(bloop::metadata::Metadata& metadata) : CByteCodeBuilder(metadata){}
	//	[[nodiscard]] constexpr bool GlobalContext() const noexcept override { return true; }
	//	bloop::BloopIndex m_uNumGlobals{};
	//};

}
