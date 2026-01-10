#include "bytecode/compile/emit.hpp"
#include "bytecode/exception.hpp"
#include "ast/function.hpp"

#include <vector>
#include <ranges>
#include <optional>
#include <iostream>
#include <ranges>
#include <algorithm>

using namespace bloop::bytecode;

bloop::BloopIndex CByteCodeBuilder::AddConstant(bloop::ConstantData c) {

	if (const auto ptr = std::ranges::find_if(m_oConstants,
		[&c](const bloop::ConstantData& p) {
			return std::get<1>(p) == std::get<1>(c) && std::get<0>(p) == std::get<0>(c);
		}); ptr != m_oConstants.end()) {

		const auto dist = std::distance(m_oConstants.begin(), ptr);

		if (static_cast<bloop::BloopIndex>(dist) > std::numeric_limits<bloop::BloopIndex>::max())
			throw exception::ByteCodeError(BLOOPTEXT("too many constants in a chunk"));

		return static_cast<bloop::BloopIndex>(std::distance(m_oConstants.begin(), ptr));
	}

	auto idx = m_oConstants.size();

	if (idx > std::numeric_limits<bloop::BloopIndex>::max())
		throw exception::ByteCodeError(BLOOPTEXT("too many constants in a chunk"));

	m_oConstants.emplace_back(c);
	return static_cast<bloop::BloopIndex>(idx);
}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopIndex arg, bloop::BloopIndex arg2, CodePosition pos) {
	m_oByteCode.emplace_back(CSingularByteCode{ .ins = Instr2{.op = opcode, .arg = arg, .arg2 = arg2 }, .loc = { m_uOffset, pos } });
	constexpr auto offset = 1 + sizeof(bloop::BloopIndex) * 2;

	if (static_cast<bloop::BloopUInt>(m_uOffset) + offset > bloop::INVALID_SLOT)
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("bytecode chunk has more than {} bytes"), bloop::INVALID_SLOT), pos);

	m_uOffset += offset;

}
void CByteCodeBuilder::Emit(EOpCode opcode, bloop::BloopIndex idx, CodePosition pos) {
	m_oByteCode.emplace_back(CSingularByteCode{ .ins = Instr1{.op = opcode, .arg = idx }, .loc = { m_uOffset, pos } });
	
	constexpr auto offset = 1 + sizeof(bloop::BloopIndex);

	if (static_cast<bloop::BloopUInt>(m_uOffset) + offset > bloop::INVALID_SLOT)
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("bytecode chunk has more than {} bytes"), bloop::INVALID_SLOT), pos);

	m_uOffset += offset;

}
void CByteCodeBuilder::Emit(EOpCode opcode, CodePosition pos) {
	m_oByteCode.emplace_back(CSingularByteCode{ .ins = Instr0{.op = opcode }, .loc = { m_uOffset, pos } });

	constexpr auto offset = 1;

	if (static_cast<bloop::BloopUInt>(m_uOffset) + offset > bloop::INVALID_SLOT)
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("bytecode chunk has more than {} bytes"), bloop::INVALID_SLOT), pos);

	m_uOffset += offset;
}
void CByteCodeBuilder::Omit() {

	if (m_oByteCode.empty())
		return;

	const auto size = m_oByteCode.back().GetBytes();
	m_oByteCode.pop_back();
	m_uOffset -= size;
}
bloop::BloopIndex CByteCodeBuilder::EmitJump(EOpCode opcode, CodePosition pos) {
	auto idx = m_oByteCode.size();
	Emit(opcode, 0, pos);
	return static_cast<bloop::BloopIndex>(idx);
}
void CByteCodeBuilder::EmitJump(EOpCode opcode, bloop::BloopIndex offset, CodePosition pos) {
	Emit(opcode, offset, pos);
}
void CByteCodeBuilder::PatchJump(bloop::BloopIndex src, bloop::BloopIndex dst) {
	std::get<1>(m_oByteCode[src].ins).arg = dst;
}
bloop::BloopIndex CByteCodeBuilder::EmitTry(EOpCode opcode, bloop::BloopIndex base, CodePosition pos) {
	auto idx = m_oByteCode.size();
	Emit(opcode, 0u, base, pos);
	return static_cast<bloop::BloopIndex>(idx);
}
void CByteCodeBuilder::PatchTry(bloop::BloopIndex src, bloop::BloopIndex dst) {
	std::get<2>(m_oByteCode[src].ins).arg = dst;
}
void CByteCodeBuilder::EmitCapture(const vmdata::Capture& capture, CodePosition pos) {
	if (capture.m_bIsLocal) {
		return Emit(EOpCode::CAPTURE_LOCAL, capture.m_uSlot, pos);
	}
	return Emit(EOpCode::CAPTURE_UPVALUE, capture.m_uSlot, pos);
}
void CByteCodeBuilder::EnsureReturn(bloop::ast::AbstractSyntaxTree* node){
	if (m_oByteCode.empty() || m_oByteCode.back().GetOpCode() != EOpCode::RETURN && m_oByteCode.back().GetOpCode() != EOpCode::RETURN_VALUE)
		Emit(EOpCode::RETURN, node->m_oApproximatePosition); //implicitly add a return statement to the end
}
void CByteCodeBuilder::AddFunction(const vmdata::Function* func) {
	m_oFunctions.push_back(func);
}
vmdata::Chunk CByteCodeBuilder::Finalize() {
	return { 
		.m_oConstants = m_oConstants, 
		.m_oByteCode = Encode(), 
		.m_oPositions = GetCodePositions(), 
		.m_oFunctions = m_oFunctions 
	};
}

void CByteCodeBuilder::Print() {

	bloop::BloopIndex ip{};
	std::ranges::for_each(m_oByteCode, [&](CSingularByteCode& c) {
		std::cout << std::dec << static_cast<bloop::BloopInt>(ip) << ": " << c.ToString() << '\n';
		ip += c.GetBytes();
	});
}


std::vector<bloop::BloopByte> CByteCodeBuilder::Encode() {
	std::vector<bloop::BloopByte> out;

	for (auto& bc : m_oByteCode) {
		std::visit([&](auto&& i) {
			out.push_back(static_cast<bloop::BloopByte>(i.op));

			if constexpr (std::is_same_v<std::decay_t<decltype(i)>, Instr1>) {
				for(const auto b : std::views::iota(0u, sizeof(bloop::BloopIndex)))
					out.push_back(static_cast<bloop::BloopByte>((i.arg >> (8 * b)) & 0xFF));
				
			}
		}, bc.ins);
	}

	if(out.size() > bloop::INVALID_SLOT)
		throw exception::ByteCodeError(bloop::fmt::format(BLOOPTEXT("bytecode has more than {} bytes"), bloop::INVALID_SLOT));

	return out;
}

std::vector<CInstructionPosition> CByteCodeBuilder::GetCodePositions()
{
	std::vector<CInstructionPosition> locs;
	locs.reserve(m_oByteCode.size());
	for (auto& bc : m_oByteCode)
		locs.push_back(bc.loc);

	return locs;
}