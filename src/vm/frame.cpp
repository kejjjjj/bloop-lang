#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "vm/frame.hpp"
#include "utils/fmt.hpp"

#include <ranges>
#include <algorithm>

using namespace bloop::vm;
CallFrame::CallFrame(Chunk* chunk, bloop::BloopUInt stackBase) 
	: m_pChunk(chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_chunk){}
CallFrame::CallFrame(Function* fn, bloop::BloopUInt stackBase)
	: m_pFunction(fn), m_pChunk(&fn->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_function) {}
CallFrame::CallFrame(Closure* closure, bloop::BloopUInt stackBase)
	: m_pClosure(closure), m_pChunk(&closure->function->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_closure) {}

const bloop::bc::InstrDebugRef& CallFrame::GetCurrentPosition(VM& vm) const {
	const auto& data = vm.m_refMetaData.m_oVMData.m_oChunks[m_pChunk->m_uMetadata];

	auto it = std::upper_bound(data.m_oInstructions.begin(), data.m_oInstructions.end(), static_cast<bloop::BloopUInt>(m_pIp - m_pIpBase),
		[](bloop::BloopUInt ip, const bc::InstrDebugRef& p) {
			return ip <= p.m_uByteOffset;
		});
	return *(it - 1);
}

void VM::PushFrame(Function* fn) {
	const auto frameBase = m_oStack.size() - fn->m_uParamCount;
	const auto needed = fn->m_uParamCount + fn->m_uLocalCount;

	if (m_oStack.size() + static_cast<bloop::BloopUInt>(needed) >= BLOOP_MAX_STACK)
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

std::vector<bloop::bc::InstrDebugRef> VM::StackTrace()
{
	std::vector<bc::InstrDebugRef> positions;
	positions.reserve(m_oFrames.size());
	while (m_oFrames.size()) {
		positions.push_back(m_oFrames.back().GetCurrentPosition(*this));
		m_oGC.CloseUpValues(m_oStack.data() + m_oFrames.back().m_uBase);
		m_oStack.resize(m_oFrames.back().m_uBase);

		PopFrame();
	}

	return positions;
}
bloop::BloopString VM::FormatStackTraceMessage(const bc::InstrDebugRef& ref)
{

	const auto trim = [](bloop::BloopString const& str) {
		static char const* whitespaceChars = "\n\r\t ";
		const auto start = str.find_first_not_of(whitespaceChars);
		const auto end = str.find_last_not_of(whitespaceChars);

		return start != bloop::BloopString::npos ? str.substr(start, 1 + end - start) : bloop::BloopString();
	};

	auto& lineData = m_refMetaData.m_oLineMap[std::get<0>(ref.m_oPosition) - 1u];
	auto offset = std::get<1>(ref.m_oPosition);

	bloop::BloopString msg = bloop::fmt::format(BLOOPTEXT("at [{}, {}]\n"), std::get<0>(ref.m_oPosition), offset);
	
	const auto trimmed = trim(bloop::BloopString(lineData));
	msg += trimmed + '\n';

	bloop::BloopUInt visualCol{};
	for (const auto i : std::views::iota(0u, offset - (lineData.length() - trimmed.length()))) {
		const auto c = lineData[i];
		visualCol += c == '\t' ? 8u - (visualCol % 8u) : 1u;
	}

	msg += bloop::BloopString(visualCol, ' ');
	msg += '^';

	return msg;
}