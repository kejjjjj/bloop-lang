#pragma once
#include "utils/defs.hpp"
#include "bytecode/defs.hpp"

#include <vector>
#include <variant>

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
		
		[[nodiscard]] bloop::BloopUInt16 AddConstant(CConstant&& c);
		void Emit(EOpCode opcode, bloop::BloopUInt16 idx);
		void Emit(EOpCode opcode);
		[[nodiscard]] bloop::BloopUInt16 EmitJump(EOpCode opcode); //returns the index of m_oByteCode
		void EmitJump(EOpCode opcode, bloop::BloopUInt16 offset);
		void PatchJump(bloop::BloopUInt16 src, bloop::BloopUInt16 dst); //dst indexes m_oByteCode
		void Print();

		[[nodiscard]] std::vector<bloop::BloopByte> Encode();

		std::vector<CConstant> m_oConstants;
		std::vector<CSingularByteCode> m_oByteCode;
		bloop::BloopUInt16 m_uOffset{};
	};

}
