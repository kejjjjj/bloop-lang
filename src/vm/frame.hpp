#pragma once

#include "vm/vm_defs.hpp"

namespace bloop::vm
{
	struct CallFrame {
		CallFrame(Chunk* chunk, bloop::BloopSize stackBase);
		CallFrame(Function* fn, bloop::BloopSize stackBase);
		CallFrame(Object* closure, bloop::BloopSize stackBase);

		[[nodiscard]] const bc::InstrDebugRef& GetCurrentPosition(class VM& vm) const;

		union {
			Function* m_pFunction{};
			Closure* m_pClosure; // == &m_pCallee->closure, the GC keeps it up to date
		};

		// the closure object being executed. The callee isn't on the stack during the call,
		// so this is a GC root (and gets updated when the object moves)
		Object* m_pCallee{};

		Chunk* m_pChunk{};
		bloop::BloopByte* m_pIp{};
		bloop::BloopByte* m_pIpBase{};
		bloop::BloopSize m_uBase{};
		std::vector<ExceptionHandler> m_oExceptionHandlers;

		enum class ChunkType : bloop::BloopByte { ct_chunk, ct_function, ct_closure };

		ChunkType m_eChunkType{ ChunkType::ct_chunk };
	};
}