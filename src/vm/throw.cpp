#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/frame.hpp"

using namespace bloop::vm;

void VM::Throw(Value value) {
	
	//unwind the stack
	while (!m_oFrames.empty()) {
		auto& frame = m_pCurrentFrame;

		if (!frame->m_oExceptionHandlers.empty()) {

			const auto& top = frame->m_oExceptionHandlers.back();

			bloop::BloopUInt newSize{};

			if (frame->m_eChunkType != CallFrame::ChunkType::ct_chunk) {
				const auto& func = frame->m_eChunkType == CallFrame::ChunkType::ct_function ? frame->m_pFunction : frame->m_pClosure->function;
				newSize = frame->m_uBase + func->m_uLocalCount + top.m_uBase;
			} else {
				newSize = frame->m_uBase + top.m_uBase;
			}

			m_oGC.CloseUpValues(m_oStack.data() + newSize);
			m_oStack.resize(newSize); //go back to the try state

			frame->m_uIp = top.m_uIp; //resume ip at catch
			m_oStack[frame->m_uBase + top.m_uCatchVar] = value;
			frame->m_oExceptionHandlers.pop_back();
			return;
		}

		m_oGC.CloseUpValues(m_oStack.data() + frame->m_uBase);
		PopFrame();
	}

	throw exception::VMError(bloop::fmt::format(BLOOPTEXT("uncaught exception: {}"), value.ValueToString()));
}