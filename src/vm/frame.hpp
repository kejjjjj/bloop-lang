#pragma once

#include "vm/vm_defs.hpp"

namespace bloop::vm
{
	struct CallFrame {
		CallFrame(Chunk* chunk, bloop::BloopUInt stackBase);
		CallFrame(Function* fn, bloop::BloopUInt stackBase);
		CallFrame(Closure* closure, bloop::BloopUInt stackBase);

		[[nodiscard]] const bc::InstrDebugRef& GetCurrentPosition(class VM& vm) const;

		union {
			Function* m_pFunction{};
			Closure* m_pClosure;
		};

		Chunk* m_pChunk{};
		bloop::BloopUInt m_uIp{};
		bloop::BloopUInt m_uBase{};
		std::vector<ExceptionHandler> m_oExceptionHandlers;

		enum class ChunkType : bloop::BloopByte { ct_chunk, ct_function, ct_closure };

		ChunkType m_eChunkType{ ChunkType::ct_chunk };
	};
}