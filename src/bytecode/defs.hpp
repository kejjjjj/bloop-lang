#pragma once

#include "lexer/punctuation.hpp"

#include <unordered_map>
#include <vector>

#ifdef NOMINMAX
#define HAD_NOMINMAX 1
#else
#define HAD_NOMINMAX 0
#endif

namespace bloop::bytecode
{
#define NOMINMAX
	static constexpr bloop::BloopUInt16 INVALID_SLOT =
		std::numeric_limits<bloop::BloopUInt16>::max();
#if (HAD_NOMINMAX == 0)
#undef NOMINMAX
#endif

	enum class EOpCode : unsigned char {
		#define BLOOP_OP(name) name,
		#include "opcode.def"
		#undef BLOOP_OP
	};

	struct CConstant {
		bloop::BloopString m_pConstant;
		bloop::EValueType m_eDataType{};
	};
	struct CInstructionPosition {
		std::size_t m_uByteOffset;
		CodePosition m_oPosition;
	};
	namespace vmdata {
		struct Function;
		struct Capture {
			bool m_bIsLocal;
			bloop::BloopUInt16 m_uSlot{};
		};
		struct Chunk {
			std::vector<CConstant> m_oConstants;
			std::size_t m_uNumGlobals{};
			std::vector<bloop::BloopByte> m_oByteCode;
			std::vector<CInstructionPosition> m_oPositions;
			std::vector<const Function*> m_oFunctions;
		};
		struct Function {
			bloop::BloopString m_sName;
			bloop::BloopUInt16 m_uParamCount{};
			bloop::BloopUInt16 m_uLocalCount{};
			Chunk chunk;
			std::vector<Capture> m_oCaptures;
		};
		
	
	}
	struct VMByteCode {
		vmdata::Chunk chunk;
		std::size_t numGlobals;
		std::vector<vmdata::Function> functions;
	};
	static std::unordered_map<EPunctuation, EOpCode> conversionTable = {
		{ EPunctuation::p_add, EOpCode::ADD },
		{ EPunctuation::p_sub, EOpCode::SUB },
		{ EPunctuation::p_multiplication, EOpCode::MUL },
		{ EPunctuation::p_division, EOpCode::DIV },
		{ EPunctuation::p_less_equal, EOpCode::LESS_EQUAL },
	};

	static std::unordered_map<EOpCode, bloop::BloopString> stringConversionTable = {
		#define BLOOP_OP(name) { EOpCode::name, #name },
		#include "opcode.def"
		#undef BLOOP_OP
	};

}
