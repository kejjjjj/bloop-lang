#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "utils/fmt.hpp"

using namespace bloop::vm;
CallFrame::CallFrame(Chunk* chunk, bloop::BloopUInt stackBase) 
	: m_pChunk(chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_chunk){}
CallFrame::CallFrame(Function* fn, bloop::BloopUInt stackBase)
	: m_pFunction(fn), m_pChunk(&fn->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_function) {}
CallFrame::CallFrame(Closure* closure, bloop::BloopUInt stackBase)
	: m_pClosure(closure), m_pChunk(&closure->function->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_closure) {}

const CInstructionPosition& CallFrame::GetCurrentPosition() const {

	auto it = std::upper_bound(m_pChunk->m_oPositions.begin(), m_pChunk->m_oPositions.end(), m_uIp,
		[](bloop::BloopUInt ip, const CInstructionPosition& p) {
			return ip <= p.byteOffset;
		});
	return *(it - 1);
}

void VM::PushFrame(Function* fn) {
	const auto frameBase = m_oStack.size() - fn->m_uParamCount;
	const auto needed = fn->m_uParamCount + fn->m_uLocalCount;

	if (m_oStack.size() + static_cast<bloop::BloopUInt>(needed) > BLOOP_MAX_STACK)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	if (m_oFrames.size() >= BLOOP_MAX_FRAMES)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} call frames"), BLOOP_MAX_FRAMES));

	m_oStack.resize(frameBase + fn->m_uLocalCount);
	m_pCurrentFrame = &m_oFrames.emplace_back(fn, frameBase);
}
void VM::PushFrame(Closure* closure) {
	const auto frameBase = m_oStack.size() - closure->function->m_uParamCount;
	const auto needed = closure->function->m_uParamCount + closure->function->m_uLocalCount;

	if (m_oStack.size() + static_cast<bloop::BloopUInt>(needed) > BLOOP_MAX_STACK)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	if (m_oFrames.size() >= BLOOP_MAX_FRAMES)
		throw exception::VMError(bloop::fmt::format(BLOOPTEXT("exceeded {} call frames"), BLOOP_MAX_FRAMES));

	m_oStack.resize(frameBase + closure->function->m_uLocalCount);
	m_pCurrentFrame = &m_oFrames.emplace_back(closure, frameBase);
}

void VM::PopFrame() {
	m_oStack.resize(m_oFrames.back().m_uBase);
	m_oFrames.pop_back();
	m_pCurrentFrame = m_oFrames.empty() ? nullptr : &m_oFrames.back();
}
void VM::Push(const Value& v) {
	m_oStack.push_back(v);
}
Value VM::Pop() {
	assert(!m_oStack.empty());
	Value v = m_oStack.back();
	m_oStack.pop_back();
	return v;
}