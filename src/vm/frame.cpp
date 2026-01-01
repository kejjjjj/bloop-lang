#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"

using namespace bloop::vm;

CallFrame::CallFrame(Chunk* fn, std::size_t stackBase) 
	: m_pChunk(fn), m_uBase(stackBase) {}
CallFrame::CallFrame(Closure* closure, std::size_t stackBase) 
	: m_pClosure(closure), m_pChunk(&closure->function->chunk), m_uBase(stackBase) {}

const CInstructionPosition& CallFrame::GetCurrentPosition() const {
	auto it = std::upper_bound(m_pChunk->m_oPositions.begin(), m_pChunk->m_oPositions.end(), m_uIp,
		[](std::size_t ip, const CInstructionPosition& p) {
			return ip <= p.byteOffset;
		});
	return *(it - 1);
}

void VM::PushFrame(Function* fn) {
	const auto frameBase = m_oStack.size() - fn->m_uParamCount;
	m_oStack.resize(frameBase + fn->m_uLocalCount);
	m_pCurrentFrame = &m_oFrames.emplace_back(&fn->chunk, frameBase);
}
void VM::PushFrame(Closure* closure) {
	const auto frameBase = m_oStack.size() - closure->function->m_uParamCount;
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