#include "vm/vm.hpp"
#include "vm/heap/dvalue.hpp"
#include "vm/exception.hpp"
#include "vm/frame.hpp"
#include "utils/fmt.hpp"

#include <ranges>
#include <algorithm>

using namespace bloop::vm;
CallFrame::CallFrame(Chunk* chunk, bloop::BloopSize stackBase) 
	: m_pChunk(chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_chunk){}
CallFrame::CallFrame(Function* fn, bloop::BloopSize stackBase)
	: m_pFunction(fn), m_pChunk(&fn->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_function) {}
CallFrame::CallFrame(Object* closure, bloop::BloopSize stackBase)
	: m_pClosure(&closure->closure), m_pCallee(closure), m_pChunk(&closure->closure.function->chunk), m_uBase(stackBase), m_eChunkType(ChunkType::ct_closure) {
	assert(closure->IsClosure());
}

const bloop::bc::InstrDebugRef& CallFrame::GetCurrentPosition(VM& vm) const {
	const auto& data = vm.m_refMetaData.m_oVMData.m_oChunks[m_pChunk->m_uMetadata];

	auto it = std::upper_bound(data.m_oInstructions.begin(), data.m_oInstructions.end(), static_cast<bloop::BloopUInt>(m_pIp - m_pIpBase),
		[](bloop::BloopUInt ip, const bc::InstrDebugRef& p) {
			return ip <= p.m_uByteOffset;
		});
	return *(it - 1);
}

void VM::StartFrame(CallFrame& frame) {
	auto& bytecode = m_refMetaData.m_oVMData.m_oChunks[frame.m_pChunk->m_uMetadata].m_oByteCode;
	frame.m_pIpBase = bytecode.data();
	frame.m_pIp = frame.m_pIpBase;
	frame.m_pIpEnd = frame.m_pIpBase + bytecode.size();
	m_pCurrentFrame = &frame;
}
void VM::CheckNativeStack() const {
	if (!m_oLimits.m_uStackBytes || !m_uStackBase)
		return;

	// the stack grows down on every platform bloop runs on
	const auto here = NativeStackAddress();

	if (here < m_uStackBase && m_uStackBase - here > m_oLimits.m_uStackBytes)
		ThrowError(bloop::fmt::format(BLOOPTEXT("stack overflow: native functions calling script functions go too deep ({} calls used more than {} bytes of the native stack)"), m_oFrames.size(), m_oLimits.m_uStackBytes));
}
void VM::PushFrame(Function* fn) {
	const auto frameBase = m_oStack.size() - fn->m_uParamCount;
	const auto needed = fn->m_uParamCount + fn->m_uLocalCount;

	if (m_oStack.size() + static_cast<bloop::BloopUInt>(needed) >= BLOOP_MAX_STACK)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	if (m_oFrames.size() >= BLOOP_MAX_FRAMES)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} call frames"), BLOOP_MAX_FRAMES));

	m_oStack.resize(frameBase + fn->m_uLocalCount);
	StartFrame(m_oFrames.emplace_back(fn, frameBase)); // (m_oFrames never reallocates: it's reserved)
}
void VM::PushFrame(Object* closureObject) {
	auto* closure = &closureObject->closure;
	const auto frameBase = m_oStack.size() - closure->function->m_uParamCount;
	const auto needed = closure->function->m_uParamCount + closure->function->m_uLocalCount;

	if (m_oStack.size() + static_cast<bloop::BloopUInt>(needed) > BLOOP_MAX_STACK)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} stack values"), BLOOP_MAX_STACK));

	if (m_oFrames.size() >= BLOOP_MAX_FRAMES)
		ThrowError(bloop::fmt::format(BLOOPTEXT("exceeded {} call frames"), BLOOP_MAX_FRAMES));

	m_oStack.resize(frameBase + closure->function->m_uLocalCount);
	StartFrame(m_oFrames.emplace_back(closureObject, frameBase));
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

	const auto line = std::get<0>(ref.m_oPosition);
	auto offset = std::get<1>(ref.m_oPosition);

	bloop::BloopString msg = bloop::fmt::format(BLOOPTEXT("at [{}, {}]\n"), line, offset);

	// with modules, lines are numbered across all files, so name the file and its own line instead
	if (m_refMetaData.m_oSourceFiles.size() > 1u) {
		if (const auto* file = m_refMetaData.FindSourceFile(line))
			msg = bloop::fmt::format(BLOOPTEXT("at [{}:{}, {}]\n"), file->m_sName, line - file->m_uFirstLine + 1u, offset);
	}

	// never crash while reporting an error
	if (line == 0u || line > m_refMetaData.m_oLineMap.size())
		return msg;

	const auto& lineData = m_refMetaData.m_oLineMap[line - 1u];

	const auto trimmed = trim(bloop::BloopString(lineData));
	msg += trimmed + '\n';

	// the lexer's column starts at 1 and counts a tab as 4, so walk the line to find the character it points at
	const auto leading = std::min(lineData.find_first_not_of(BLOOPTEXT("\n\r\t ")), lineData.length());
	std::size_t index{};
	for (std::size_t column = 1u; index < lineData.length() && column < offset; index++)
		column += lineData[index] == '\t' ? 4u : 1u;

	// the caret goes under that character in the trimmed line
	bloop::BloopSize visualCol{};
	for (auto i = leading; i < index && i - leading < trimmed.length(); i++)
		visualCol += lineData[i] == '\t' ? 8u - (visualCol % 8u) : 1u;

	msg += bloop::BloopString(visualCol, ' ');
	msg += '^';

	return msg;
}