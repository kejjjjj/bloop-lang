#pragma once
#include "utils/defs.hpp"
#include "bytecode/defs.hpp"

#include <vector>
#include <variant>

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
		bloop::BloopUInt16 arg;
	};
	using Instruction = std::variant<Instr0, Instr1>;



	struct CSingularByteCode {		
		Instruction ins;
		CInstructionPosition loc; // for runtime error messages

		[[nodiscard]] bloop::BloopUInt16 GetBytes() const {
			bloop::BloopUInt16 result{};
			std::visit([&](auto&& i) {
				
				result = 1;
				if (std::is_same_v<Instr1, std::decay_t<decltype(i)>>)
					result = 3;

				}, ins);
			return result;
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
				res = stringConversionTable[i.op];

				if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr1>) {
					res += ", " + std::to_string(i.arg);
				}

			}, ins);

			return res;
		}
	};


	struct CByteCodeBuilder {
		CByteCodeBuilder(std::vector<vmdata::Function>& allFuncs) : m_oAllFunctions(allFuncs){}

		virtual ~CByteCodeBuilder() = default;
		[[nodiscard]] virtual constexpr bool GlobalContext() const noexcept { return false; }
		[[nodiscard]] bloop::BloopUInt16 AddConstant(CConstant&& c);
		void Emit(EOpCode opcode, bloop::BloopUInt16 idx, CodePosition pos);
		void Emit(EOpCode opcode, CodePosition pos);
		[[nodiscard]] bloop::BloopUInt16 EmitJump(EOpCode opcode, CodePosition pos); //returns the index of m_oByteCode
		void EmitJump(EOpCode opcode, bloop::BloopUInt16 offset, CodePosition pos);
		void PatchJump(bloop::BloopUInt16 src, bloop::BloopUInt16 dst); //dst indexes m_oByteCode
		void EmitCapture(const vmdata::Capture& capture, CodePosition pos);
		void EnsureReturn(bloop::ast::AbstractSyntaxTree* node);
		void AddFunction(const vmdata::Function* func);
		[[nodiscard]] inline auto FunctionCount() const noexcept { return m_oFunctions.size(); }
		[[nodiscard]] vmdata::Chunk Finalize();

		void Print();

		[[nodiscard]] std::vector<bloop::BloopByte> Encode();
		[[nodiscard]] std::vector<CInstructionPosition> GetCodePositions();

		std::vector<CConstant> m_oConstants;
		std::vector<CSingularByteCode> m_oByteCode;
		bloop::BloopUInt16 m_uOffset{};

		std::vector<vmdata::Function>& m_oAllFunctions;

	private:
		std::vector<const vmdata::Function*> m_oFunctions; // references m_oAllFunctions
	};

	struct CByteCodeBuilderForGlobals : CByteCodeBuilder {
		CByteCodeBuilderForGlobals(std::vector<vmdata::Function>& f) : CByteCodeBuilder(f){}
		[[nodiscard]] constexpr bool GlobalContext() const noexcept override { return true; }
		bloop::BloopUInt16 m_uNumGlobals{};
	};

}
