#pragma once

#include "utils/defs.hpp"
#include <vector>

namespace bloop::bc
{
	using DebugId = bloop::BloopIndex;

    struct Capture {
        bloop::BloopIndex m_uSlot;
        bloop::BloopBool m_bIsLocal;
    };
	struct InstrDebugRef {
		bloop::BloopIndex m_uByteOffset;
		bloop::CodePosition m_oPosition;
	};

	struct Chunk {
		DebugId m_uId;
		std::vector<bloop::ConstantData> m_oConstants;
		std::vector<bloop::BloopByte> m_oByteCode;
		std::vector<InstrDebugRef> m_oInstructions;
	};

	struct Function {
		DebugId m_uId;
		bloop::BloopIndex m_uChunkIndex;
		bloop::BloopIndex m_uParamCount;
		bloop::BloopIndex m_uLocalCount;
		std::vector<bc::Capture> m_oCaptures;
	};

}