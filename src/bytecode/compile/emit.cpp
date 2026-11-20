#include "bytecode/compile/emit.hpp"
#include "bytecode/exception.hpp"
#include "ast/function.hpp"

#include <vector>
#include <ranges>
#include <optional>
#include <iostream>
#include <ranges>
#include <algorithm>
#include <type_traits>
#include <iterator>
#include <limits>
#include <cassert>
#include <utility>

using namespace bloop::bytecode;

bloop::BloopIndex CByteCodeBuilder::AddConstant(bloop::ConstantData c) {

	if (const auto ptr = std::ranges::find_if(m_oConstants,
		[&c](const bloop::ConstantData& p) {
			return std::get<1>(p) == std::get<1>(c) && std::get<0>(p) == std::get<0>(c);
		}); ptr != m_oConstants.end()) {

		return static_cast<bloop::BloopIndex>(std::distance(m_oConstants.begin(), ptr));
	}

	const auto idx = m_oConstants.size();

	if (idx >= bloop::INVALID_SLOT)
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("more than {} constants in a chunk"), bloop::INVALID_SLOT));

	m_oConstants.emplace_back(c);
	return static_cast<bloop::BloopIndex>(idx);
}
void CByteCodeBuilder::Add(Instruction ins, CodePosition pos) {
	CSingularByteCode bc{ .ins = ins, .loc = { m_uOffset, pos } };
	const auto size = bc.GetBytes();

	if (static_cast<bloop::BloopUInt>(m_uOffset) + size >= std::numeric_limits<bloop::BloopOffset>::max())
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("bytecode chunk has more than {} bytes"), std::numeric_limits<bloop::BloopOffset>::max()), pos);
	if (m_oByteCode.size() >= std::numeric_limits<bloop::BloopOffset>::max())
		throw exception::ByteCodeError(BLOOPTEXT("too many instructions in a chunk"), pos);

	m_oByteCode.emplace_back(std::move(bc));
	m_uOffset += size;
}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopIndex arg, bloop::BloopIndex arg2, bloop::BloopIndex arg3, CodePosition pos) {
	Add(Instr3{ .op = opcode, .arg = arg, .arg2 = arg2, .arg3 = arg3 }, pos);
}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopIndex arg, bloop::BloopIndex arg2, CodePosition pos) {
	Add(Instr2{ .op = opcode, .arg = arg, .arg2 = arg2 }, pos);
}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopIndex idx, CodePosition pos) {
	Add(Instr1{ .op = opcode, .arg = idx }, pos);
}
void CByteCodeBuilder::Emit(EOpCode opcode, CodePosition pos) {
	Add(Instr0{ .op = opcode }, pos);
}
bloop::BloopOffset CByteCodeBuilder::EmitJump(EOpCode opcode, CodePosition pos) {
	const auto idx = static_cast<bloop::BloopOffset>(m_oByteCode.size());
	Add(InstrJump{ .op = opcode, .target = 0u }, pos);
	return idx;
}
void CByteCodeBuilder::EmitJump(EOpCode opcode, bloop::BloopOffset target, CodePosition pos) {
	Add(InstrJump{ .op = opcode, .target = target }, pos);
}
void CByteCodeBuilder::PatchJump(bloop::BloopOffset src, bloop::BloopOffset dst) {
	std::get<InstrJump>(m_oByteCode[src].ins).target = dst;
}
bloop::BloopOffset CByteCodeBuilder::EmitTry(EOpCode opcode, bloop::BloopIndex catchVar, CodePosition pos) {
	const auto idx = static_cast<bloop::BloopOffset>(m_oByteCode.size());
	Add(InstrJumpArg{ .op = opcode, .target = 0u, .arg = catchVar }, pos);
	return idx;
}
void CByteCodeBuilder::PatchTry(bloop::BloopOffset src, bloop::BloopOffset dst) {
	std::get<InstrJumpArg>(m_oByteCode[src].ins).target = dst;
}
void CByteCodeBuilder::EmitCapture(const bc::Capture& capture, CodePosition pos) {
	if (capture.m_bIsGlobal)
		return Emit(EOpCode::CAPTURE_GLOBAL, capture.m_uSlot, pos);

	if (capture.m_bIsLocal) {
		return Emit(EOpCode::CAPTURE_LOCAL, capture.m_uSlot, pos);
	}
	return Emit(EOpCode::CAPTURE_UPVALUE, capture.m_uSlot, pos);
}
void CByteCodeBuilder::EnsureReturn(bloop::ast::AbstractSyntaxTree* node){
	if (m_oByteCode.empty() || ((m_oByteCode.back().GetOpCode() != EOpCode::RETURN) && (m_oByteCode.back().GetOpCode() != EOpCode::RETURN_VALUE)))
		Emit(EOpCode::RETURN, node->m_oApproximatePosition); //implicitly add a return statement to the end
}
void CByteCodeBuilder::AddFunction(bloop::BloopIndex idx) {
	m_oFunctions.push_back(idx);
}
bloop::bc::Chunk CByteCodeBuilder::Finalize() {

	bc::Chunk c;
	c.m_oByteCode = Encode();
	c.m_oConstants = m_oConstants;
	c.m_oInstructions = GetCodePositions();
	return c;
}

void CByteCodeBuilder::Print() {

	bloop::BloopOffset ip{};
	std::ranges::for_each(m_oByteCode, [&](CSingularByteCode& c) {
		std::cout << std::dec << static_cast<bloop::BloopInt>(ip) << ": " << c.ToString() << '\n';
		ip += c.GetBytes();
	});
}

namespace {
	template <typename T>
	inline void WriteLE(std::vector<bloop::BloopByte>& out, T v) {
		static_assert(std::is_integral_v<T>, "T must be integral");
		for(const auto i : std::views::iota(0u, sizeof(T)))
			out.push_back(static_cast<bloop::BloopByte>((v >> (8 * i)) & 0xFF));
	}

	template <typename Instr>
	inline void EncodeInstruction(std::vector<bloop::BloopByte>& out, const Instr& i) {
		out.push_back(static_cast<bloop::BloopByte>(i.op));
		if constexpr (requires { i.target; })  WriteLE(out, i.target);
		if constexpr (requires { i.arg; })  WriteLE(out, i.arg);
		if constexpr (requires { i.arg2; }) WriteLE(out, i.arg2);
		if constexpr (requires { i.arg3; }) WriteLE(out, i.arg3);
	}
}

std::vector<bloop::BloopByte> CByteCodeBuilder::Encode() {
	std::vector<bloop::BloopByte> out;

	out.reserve(m_oByteCode.size() * (1 + 3 * sizeof(bloop::BloopIndex)));

	for (auto& bc : m_oByteCode) {
		std::visit([&](const auto& i) {
			EncodeInstruction(out, i);
		}, bc.ins);
	}

	assert(out.size() == m_uOffset);
	return out;
}

std::vector<bloop::bc::InstrDebugRef> CByteCodeBuilder::GetCodePositions()
{
	std::vector<bc::InstrDebugRef> locs;
	locs.reserve(m_oByteCode.size());
	for (auto& bc : m_oByteCode)
		locs.push_back(bc.loc);

	return locs;
}