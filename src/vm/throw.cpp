#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"
#include "vm/frame.hpp"

using namespace bloop::vm;

void VM::Throw(Value value) {
    //assert(!m_oStack.empty() && "Throw called with empty stack");

    while (!m_oFrames.empty()) {
        auto& frame = m_pCurrentFrame;
        assert(frame != nullptr && "Current frame pointer is null while frames exist");
        assert(frame == &m_oFrames.back() && "m_pCurrentFrame does not point to top of frame stack");

        [[maybe_unused]] const auto& bytecode = m_refMetaData.m_oVMData.m_oChunks[frame->m_pChunk->m_uMetadata].m_oByteCode;

        assert(frame->m_uBase <= m_oStack.size() && "Frame base is beyond current stack size");
        assert(frame->m_uBase >= 0 && "Frame base is negative");
        assert(frame->m_pChunk != nullptr && "Frame has no chunk");
        assert(frame->m_uIp <= bytecode.size() && "IP out of bounds in chunk");

        if (!frame->m_oExceptionHandlers.empty()) {
            const auto& top = frame->m_oExceptionHandlers.back();

            assert(top.m_uBase <= m_oStack.size() && "TRY base is beyond current stack size");
            assert(top.m_uBase >= frame->m_uBase && "TRY base is below current frame base");
            assert(top.m_uCatchVar < 1000 && "Catch variable index looks unreasonably large"); // arbitrary reasonable limit
            assert(top.m_uIp < bytecode.size() && "Catch IP is out of bounds");

            assert(m_oStack.size() >= top.m_uBase && "Stack size already smaller than TRY base");


            m_oStack.resize(top.m_uBase);

#if (DEBUG || _DEBUG)
            m_oGC.CheckUpValueList();
#endif

            assert(m_oStack.size() == top.m_uBase && "Stack resize did not set expected size");
            assert(frame->m_uBase <= m_oStack.size() && "Frame base now invalid after resize");

            const auto catch_slot = frame->m_uBase + top.m_uCatchVar;
            assert(catch_slot >= frame->m_uBase && "Catch slot below frame base");

            if (m_oStack.empty()) {
                m_oGlobals[catch_slot] = value;
            } else {
                assert(catch_slot < m_oStack.size() && "Catch variable slot is out of bounds after resize");
                m_oStack[catch_slot] = value;
            }

            frame->m_uIp = top.m_uIp;
            assert(frame->m_uIp < bytecode.size() && "Restored IP out of bounds");

            frame->m_oExceptionHandlers.pop_back();

            assert(!frame->m_oExceptionHandlers.empty() || frame->m_oExceptionHandlers.size() == 0);
            return; 
        }

        assert(frame->m_uBase <= m_oStack.size() && "Frame base exceeds stack size before close");
        m_oGC.CloseUpValues(m_oStack.data() + frame->m_uBase);
        PopFrame();

        if (!m_oFrames.empty()) {
            assert(m_pCurrentFrame == &m_oFrames.back() && "Current frame not updated after PopFrame");
            assert(m_pCurrentFrame->m_uBase <= m_oStack.size() && "New frame base exceeds stack after pop");
        } else {
            assert(m_pCurrentFrame == nullptr && "Current frame not cleared after last pop");
        }
    }

    throw exception::VMError(bloop::fmt::format(BLOOPTEXT("uncaught exception: {}"), value.ValueToString()));
}